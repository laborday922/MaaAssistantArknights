#pragma once
#include "Task/InterfaceTask.h"

namespace asst
{
class ProcessTask;
class AccountSwitchTask;

class AwardTask final : public InterfaceTask
{
public:
    inline static constexpr std::string_view TaskType = "Award";

    AwardTask(const AsstCallback& callback, Assistant* inst);
    virtual ~AwardTask() override = default;

    virtual bool set_params(const json::value& params) override;
    virtual bool run() override;

private:
    void rebuild_subtasks();

    std::shared_ptr<ProcessTask> award_task_ptr = nullptr;
    std::shared_ptr<ProcessTask> mail_task_ptr = nullptr;
    std::shared_ptr<ProcessTask> recruit_task_ptr = nullptr;
    std::shared_ptr<ProcessTask> orundum_task_ptr = nullptr;
    std::shared_ptr<ProcessTask> mining_task_ptr = nullptr;
    std::shared_ptr<ProcessTask> specialaccess_task_ptr = nullptr;

    bool m_award_enabled = true;
    bool m_mail_enabled = false;
    bool m_recruit_enabled = false;
    bool m_orundum_enabled = false;
    bool m_mining_enabled = false;
    bool m_specialaccess_enabled = false;

    int m_switch_times = 0;
    std::string m_client_type;
    bool m_logout_after = false;
};
}
