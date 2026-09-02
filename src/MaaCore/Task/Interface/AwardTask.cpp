#include "AwardTask.h"

#include <utility>

#include "Config/GeneralConfig.h"
#include "Task/Miscellaneous/AccountSwitchTask.h"
#include "Task/Miscellaneous/ScreenshotTaskPlugin.h"
#include "Task/ProcessTask.h"

#include "Utils/Logger.hpp"

asst::AwardTask::AwardTask(const AsstCallback& callback, Assistant* inst) :
    InterfaceTask(callback, inst, TaskType)
{
    LogTraceFunction;
    rebuild_subtasks();
}

bool asst::AwardTask::set_params(const json::value& params)
{
    LogTraceFunction;

    m_award_enabled = params.get("award", true);
    m_mail_enabled = params.get("mail", false);
    m_recruit_enabled = params.get("recruit", false);
    m_orundum_enabled = params.get("orundum", false);
    m_mining_enabled = params.get("mining", false);
    m_specialaccess_enabled = params.get("specialaccess", false);

    m_switch_times = params.get("switch_times", 0);
    m_client_type = params.get("client_type", std::string {});
    m_logout_after = params.get("logout_after", m_switch_times > 0);

    award_task_ptr->set_enable(m_award_enabled);
    mail_task_ptr->set_enable(m_mail_enabled);
    recruit_task_ptr->set_enable(m_recruit_enabled);
    orundum_task_ptr->set_enable(m_orundum_enabled);
    mining_task_ptr->set_enable(m_mining_enabled);
    specialaccess_task_ptr->set_enable(m_specialaccess_enabled);

    return true;
}

bool asst::AwardTask::run()
{
    LogTraceFunction;

    if (!m_enable) {
        Log.info("task disabled, pass", basic_info().to_string());
        return true;
    }

    if (m_switch_times <= 0) {
        return InterfaceTask::run();
    }

    if (m_client_type.empty()) {
        m_client_type = "Bilibili";
    }
    if (m_client_type != "Bilibili") {
        LogError << __FUNCTION__ << "switch_times requires client_type=Bilibili, got" << m_client_type;
        return false;
    }

    for (int i = 0; i < m_switch_times && !need_exit(); ++i) {
        AccountSwitchTask switch_task(m_callback, m_inst, TaskType);
        switch_task.set_retry_times(0);
        switch_task.set_client_type(m_client_type);
        switch_task.set_switch_oldest_bilibili(true);
        if (!switch_task.run()) {
            return false;
        }

        if (need_exit()) {
            return false;
        }

        ProcessTask start_up_task(m_callback, m_inst, TaskType);
        start_up_task
            .set_tasks({ "StartAtHome", "StartWithSanity", "SwitchTheme@ToggleSettingsMenu", "StartUpBegin" })
            .set_times_limit("ReturnButton", 0)
            .set_times_limit("StartButton1", 0)
            .set_task_delay(Config.get_options().task_delay * 2)
            .set_retry_times(50);
        if (!start_up_task.run()) {
            return false;
        }

        if (need_exit()) {
            return false;
        }

        rebuild_subtasks();
        if (!InterfaceTask::run()) {
            return false;
        }

        if (need_exit()) {
            return false;
        }

        if (m_logout_after) {
            ProcessTask logout_task(m_callback, m_inst, TaskType);
            logout_task.set_tasks({ "SwitchAccount@StartUpBegin" }).set_retry_times(30);
            if (!logout_task.run()) {
                return false;
            }
        }
    }

    return !need_exit();
}

void asst::AwardTask::rebuild_subtasks()
{
    award_task_ptr = std::make_shared<ProcessTask>(m_callback, m_inst, TaskType);
    mail_task_ptr = std::make_shared<ProcessTask>(m_callback, m_inst, TaskType);
    recruit_task_ptr = std::make_shared<ProcessTask>(m_callback, m_inst, TaskType);
    orundum_task_ptr = std::make_shared<ProcessTask>(m_callback, m_inst, TaskType);
    mining_task_ptr = std::make_shared<ProcessTask>(m_callback, m_inst, TaskType);
    specialaccess_task_ptr = std::make_shared<ProcessTask>(m_callback, m_inst, TaskType);

    award_task_ptr->set_tasks({ "AwardBegin" }).set_enable(m_award_enabled);
    mail_task_ptr->set_tasks({ "MailBegin" }).set_enable(m_mail_enabled);
    recruit_task_ptr->set_tasks({ "RecruitingActivitiesBegin" }).set_enable(m_recruit_enabled);
    orundum_task_ptr->set_tasks({ "OrundumActivitiesBegin" }).set_enable(m_orundum_enabled);
    mining_task_ptr->set_tasks({ "MiningActivitiesBegin" }).set_enable(m_mining_enabled);
    specialaccess_task_ptr->set_tasks({ "SpecialAccessActivitiesBegin" }).set_enable(m_specialaccess_enabled);

    award_task_ptr->register_plugin<ScreenshotTaskPlugin>();

    m_subtasks.clear();
    m_subtasks.emplace_back(award_task_ptr);
    m_subtasks.emplace_back(mail_task_ptr);
    m_subtasks.emplace_back(recruit_task_ptr);
    m_subtasks.emplace_back(orundum_task_ptr);
    m_subtasks.emplace_back(mining_task_ptr);
    m_subtasks.emplace_back(specialaccess_task_ptr);
}
