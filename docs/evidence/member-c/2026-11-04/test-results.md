# 成员 C D9 完整回归记录

平台: WSL2 Ubuntu 22.04 / Qt 6.12.0 gcc_64 / QT_QPA_PLATFORM=offscreen
入口: scripts/dev/run-d3-tests.sh（全部 10 套件）
基线: origin/main d9ff2a4f（已含 #48）

| 套件 | passed | failed | skipped |
| --- | --- | --- | --- |
| archive | 13 | 0 | 0 |
| priority | 8 | 0 | 0 |
| menu | 11 | 0 | 0 |
| modellist | 5 | 0 | 0 |
| panel | 15 | 0 | 0 |
| preferences | 9 | 0 | 0 |
| theme | 14 | 0 | 0 |
| chain | 9 | 0 | 0 |
| localization | 22 | 0 | 0 |
| files | 9 | 0 | 0 |
| **合计** | **115** | **0** | **0** |

退出码: EXIT=0

修复前基线 112 项全绿；本次 menu 套件新增 3 项，故 115 项。
全部为宿主(桌面 offscreen)结论，不是真机结论，见 MEMBER_C_D9_DEVICE_HANDOFF.md。
