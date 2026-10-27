# BOF-Pack
BOF package by [@wsummerhill](https://github.com/wsummerhill)

## BOFs

| Name | Command(s) | MITRE Tactic | Notes |
| --- | --- | --- | --- |
| EnumFiles | `enumfiles <show \| all \| lolbins \| remoting \| dotnet \| browser-installs \| browser-userdata \| webservers \| powershell-hist \| python \| unattended>` | Discovery (TA0007 / T1083, T1518) | C++ BOF to enumerate local files and directories of interest for post-exploitation (LOLbins, remoting tools, browser data/installs, web servers, PowerShell history, Python, unattended install files). Remains silent unless matches are found. Requires `%APPDATA%`, `%LOCALAPPDATA%`, and `%WINDIR%` env variables. |
| ScheduleTaskMod | `schtaskmod add <task_name_or_path> <new_program_path> [args]`<br><br>`schtaskmod remove <task_name_or_path>` | Persistence (TA0003), Execution (TA0002) / T1053.005 | C++ BOF to modify existing scheduled tasks for persistence. `add` appends an action to execute alongside the original task action for persistence. `remove` removes the last action (fails if only one action remains on the task). |

---
Credits:

[GitHub - wsummerhill](https://github.com/wsummerhill) | [X/Twitter - @BSummerz](https://x.com/BSummerz)
