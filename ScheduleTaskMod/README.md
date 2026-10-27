# BOF-ScheduleTaskMod
C++ BOF implementation to modify existing scheduled tasks for persistence.<br />

### `add` action

Syntax: `schtaskmod add <task_name_or_path> <new_program_path> [optional_arguments]`

Append a new task action to an existing scheduled task. This will allow the scheduled task to simultaneously run the original task action and the newly added task action for persistence.

### `remove` action

Syntax: `schtaskmod remove <task_name_or_path>`

Remove the last action of an existing scheduled task. This action will check if the task has more than one action present, and it will fail if there is only one action on a task since any task should never have zero actions.


## Commands Usage
The following command examples are supported:

|Command|Decription|
|----|----------|
|`schtaskmod add \"MyTask\" \"C:\\windows\\system32\\cmd.exe\" \"/c notepad.exe\"` | Add action to existing task "MyTask" |
|`schtaskmod add "Microsoft\Windows\Defrag\ScheduledDefrag" "C:\windows\system32\calc.exe"`| Add action to existing task at the task path "Microsoft\Windows\Defrag\ScheduledDefrag" |
|`schtaskmod remove "ModifiedTask"` | Remove last action from the task "ModifiedTask" |
|`schtaskmod remove "Microsoft\Windows\Defrag\ScheduledDefrag"` | Remove last action from the task at the path "Microsoft\Windows\Defrag\ScheduledDefrag" |
