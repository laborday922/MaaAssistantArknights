#include "AccountSwitchTask.h"

#include <meojson/json.hpp>

#include "Config/TaskData.h"
#include "Controller/Controller.h"
#include "MaaUtils/ImageIo.h"
#include "Task/ProcessTask.h"
#include "Utils/Logger.hpp"
#include "Vision/MultiMatcher.h"
#include "Vision/RegionOCRer.h"

namespace
{
inline void account_switch_step(asst::AccountSwitchTask& self, std::string step, json::object details = {})
{
    json::value info = self.basic_info_with_what("AccountSwitchStep");
    details["step"] = std::move(step);
    info["details"] = std::move(details);
    self.callback(asst::AsstMsg::SubTaskExtraInfo, info);
}
}

bool asst::AccountSwitchTask::_run()
{
    LogTraceFunction;

    if (need_exit()) {
        return false;
    }

    if (m_switch_oldest_bilibili) {
        if (m_client_type != "Bilibili") {
            LogError << __FUNCTION__ << "switch_oldest_bilibili requires client_type=Bilibili, got" << m_client_type;
            return false;
        }
    }
    else if (m_account.empty()) {
        Log.error(__FUNCTION__, "account is empty");
        return false;
    }

    if (std::ranges::find(SupportedClientType, m_client_type) == SupportedClientType.end()) {
        LogError << __FUNCTION__ << "unsupported client" << m_client_type;
        return false;
    }

    // 退出到选择账号界面
    account_switch_step(*this, "NavigateToAccountManager.Begin", json::object { { "client_type", m_client_type } });
    if (!navigate_to_start_page()) {
        account_switch_step(*this, "NavigateToAccountManager.Failed");
        return false;
    }
    account_switch_step(*this, "NavigateToAccountManager.Done");
    if (m_switch_oldest_bilibili) {
        account_switch_step(*this, "ShowAccountList.Begin");
        if (!show_account_list()) {
            account_switch_step(*this, "ShowAccountList.Failed");
            return false;
        }
        account_switch_step(*this, "ShowAccountList.Done");
        if (!swipe_to_bottom()) {
            account_switch_step(*this, "SwipeToBottom.Failed");
            return false;
        }
        if (!select_oldest_account_bilibili()) {
            account_switch_step(*this, "SelectOldestAccount.Failed");
            return false;
        }
        if (!click_manager_login_button()) {
            account_switch_step(*this, "ClickLoginButton.Failed");
            return false;
        }
        account_switch_step(*this, "ClickLoginButton.Done", json::object { { "account_name", m_target_account } });
        json::value info = basic_info_with_what("AccountSwitch");
        info["details"]["account_name"] = m_target_account;
        callback(AsstMsg::SubTaskExtraInfo, info);
        return true;
    }
    else {
        // 当前账号就是想要的
        bool equal = false;

        Log.info(m_client_type);
        if (m_client_type == "Official" || m_client_type == "txwy" || m_client_type == "YoStarKR") {
            equal = equal_current_account();
        }
        else if (m_client_type == "Bilibili") {
            equal = equal_current_account_b();
        }

        if (equal) {
            return click_manager_login_button();
        }
        // 展开列表
        show_account_list();

        if (swipe_and_select() || swipe_and_select(true)) {
            json::value info = basic_info_with_what("AccountSwitch");
            // info["details"]["current_account"] = current_account;
            info["details"]["account_name"] = m_target_account;
            callback(AsstMsg::SubTaskExtraInfo, info);

            return true;
        }
        else {
            return false;
        }
    }
}

bool asst::AccountSwitchTask::navigate_to_start_page()
{
    auto task = ProcessTask(*this, { "SwitchAccount@StartUpBegin" });
    task.set_retry_times(30);
    task.run();
    std::string last_name = task.get_last_task_name();
    account_switch_step(
        *this,
        "NavigateToAccountManager.LastNode",
        json::object { { "last_task", last_name }, { "client_type", m_client_type } });
    if (last_name == "LoginOther") {
        return true;
    }
    else if (last_name == "AccountManagerOfficial") {
        return true;
    }
    else if (last_name == "AccountManagerBili") {
        return true;
    }
    else if (last_name == "AccountManagerTxwy") {
        return true;
    }
    else if (last_name == "AccountManagerYostar") {
        return true;
    }
    return false;
}

bool asst::AccountSwitchTask::equal_current_account()
{
    OCRer ocr(ctrler()->get_image());
    ocr.set_task_info("AccountCurrentOCR");
    ocr.set_required({ m_account });
    if (!ocr.analyze()) {
        return false;
    }
    return true;
}

bool asst::AccountSwitchTask::equal_current_account_b()
{
    OCRer ocr(ctrler()->get_image());
    ocr.set_task_info("AccountCurrentOCRBili");
    ocr.set_required({ m_account });
    if (!ocr.analyze()) {
        return false;
    }
    return true;
}

bool asst::AccountSwitchTask::click_manager_login_button()
{
    return ProcessTask(*this, { "AccountManagerLoginButton", "AccountManagerLoginButtonBili" })
        .set_retry_times(3)
        .run();
}

bool asst::AccountSwitchTask::show_account_list()
{
    auto task = ProcessTask(*this, { "AccountManagerListAccount", "AccountManagerListAccountBili" });
    bool ok = task.run();
    account_switch_step(
        *this,
        "ShowAccountList.LastNode",
        json::object { { "ok", ok }, { "last_task", task.get_last_task_name() }, { "client_type", m_client_type } });
    return ok;
}

bool asst::AccountSwitchTask::swipe_and_select(bool to_top)
{
    if (need_exit()) {
        return false;
    }
    // 下滑寻找账号
    int repeat = 0;
    bool click = false;
    while (!need_exit()) {
        click = select_account();
        if (click) {
            return click_manager_login_button();
        }
        if (repeat++ > 20) {
            // 没找到对应账号
            return false;
        }
        swipe_account_list(to_top);
    }
    return false;
}

void asst::AccountSwitchTask::swipe_account_list(bool to_top)
{
    ProcessTask(*this, { to_top ? "AccountListSwipeToTop" : "AccountListSwipe" }).run();
}

bool asst::AccountSwitchTask::select_account()
{
    LogTraceFunction;

    sleep(200);
    auto raw_img = ctrler()->get_image();
    OCRer ocr(ctrler()->get_image());
    if (m_client_type == "Official" || m_client_type == "txwy" || m_client_type == "YoStarKR") {
        ocr.set_use_char_model(true);
    }
    else if (m_client_type == "Bilibili") {
    }
    ocr.set_required({ m_account });
    if (!ocr.analyze()) {
        return false;
    }

    asst::Rect roi = ocr.get_result().front().rect;
    m_target_account = ocr.get_result().front().text;
    ctrler()->click(roi);
    return true;
}

bool asst::AccountSwitchTask::swipe_to_bottom()
{
    LogTraceFunction;

    int repeat = 0;
    account_switch_step(*this, "SwipeToBottom.Begin");
    while (!need_exit()) {
        if (repeat++ >= 25) {
            account_switch_step(*this, "SwipeToBottom.Done", json::object { { "swipe_times", repeat } });
            return true;
        }
        swipe_account_list(false);
        if (repeat == 1 || repeat % 5 == 0) {
            account_switch_step(*this, "SwipeToBottom.Progress", json::object { { "swipe_times", repeat } });
        }
        sleep(150);
    }
    return false;
}

bool asst::AccountSwitchTask::select_oldest_account_bilibili()
{
    LogTraceFunction;

    sleep(200);
    OCRer ocr(ctrler()->get_image());
    ocr.set_roi(Rect(150, 100, 980, 560));
    ocr.set_required({});
    if (!ocr.analyze()) {
        account_switch_step(*this, "SelectOldestAccount.OCRFailed");
        return false;
    }

    const auto& results = ocr.get_result();
    const auto has_any = [](const std::string& s, const std::initializer_list<std::string_view>& needles) -> bool {
        for (auto n : needles) {
            if (s.find(n) != std::string::npos) {
                return true;
            }
        }
        return false;
    };
    const auto looks_like_bili_account = [](std::string s) -> bool {
        if (s.empty()) {
            return false;
        }
        for (auto& ch : s) {
            if (ch == ' ') {
                ch = '_';
            }
        }
        if (!s.starts_with("bili_")) {
            return false;
        }
        if (s.size() < 8) {
            return false;
        }
        int digits = 0;
        for (size_t i = 5; i < s.size(); ++i) {
            if (s[i] >= '0' && s[i] <= '9') {
                ++digits;
                continue;
            }
            if (s[i] == '_') {
                continue;
            }
            return false;
        }
        return digits >= 6;
    };

    bool found = false;
    Rect best {};
    std::string best_text;
    int best_bottom = -1;
    for (const auto& r : results) {
        const auto& text = r.text;
        if (text.size() < 2 || text.size() > 32) {
            continue;
        }
        if (!looks_like_bili_account(text)) {
            continue;
        }
        if (has_any(text, { "登录", "记录", "上次", "切换", "账号", "退出", "确定", "返回", "删除", "管理" })) {
            continue;
        }
        int bottom = r.rect.y + r.rect.height;
        if (bottom > best_bottom) {
            best_bottom = bottom;
            best = r.rect;
            best_text = text;
            found = true;
        }
    }

    if (!found) {
        account_switch_step(*this, "SelectOldestAccount.NotFound");
        return false;
    }

    m_target_account = best_text;
    account_switch_step(*this, "SelectOldestAccount.Found", json::object { { "account_name", m_target_account } });
    ctrler()->click(best);
    return true;
}
