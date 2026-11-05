# D9 组长真机回归记录（2026-11-04）

本轮已完成新包构建、签名、覆盖安装、启动/二次启动、系统文件选择取消、坏文件拒绝、合法空模型与中文路径 MPD 保存重开，以及本次工具栏修复的真机检查。组长亲手连续三轮同进程 LDR→MPD，反馈“不卡，正常”。本轮没有观察到新的卡死。

**D9 组长当日回归工作已完成；未测项和现有阻断项按下表交接。此记录不等于全部真机矩阵通过或 Alpha 已放行。** #45 仍待 A/B 结合新证据复核；#15 的真实拒绝授权、不可写目标及恢复体验尚未完成。未自动关闭 Issue、推送分支或创建 PR。按组长要求未录屏。

## 1. 本轮被测包与设备

| 项目 | 值 |
|---|---|
| 源码 | `2c514fcbfffc8282ae53ffdbe20de62aed21f881`（main，包含 C #52、A #53、B #54） |
| 构建 | Release，`LEOCAD_HARMONY_SAFE_RENDER=OFF`，model-test HAP OFF |
| 工具链 | Qt 6.12.0；SDK 26.0.0.105；CLI 26.0.0.851 |
| HAP | `leocad-d9-signed.hap`，80,139,912 bytes |
| HAP SHA-256 | `db69d9f1e4e3dc0fd6d80c4e87b4a1ab90477fabd59bc722bd48f2b0f8fdf54e` |
| Native Build ID | `16c20e3b70e5573652735e73b3320e294c68a688` |
| 应用 | `io.github.xiaotaop.leocad` / `QAbility` |
| 设备 | HAD-W32；系统参数 OpenHarmony-6.1.0.115；API 23；屏幕 3120×2080 |
| 输入 | 组长自带键盘/触控板完成三轮手工操作；其余补充步骤由 HDC UI 注入 |
| 新包进程 | PID 11449 → 正常退出；PID 16935 → 正常退出 |
| 实际批次 | 22:35 启动新包，23:02 结束第二进程；未将其写成计划的 21:00 |

收据：[build-receipt.json](evidence/leader/2026-11-04/d9-device/build-receipt.json)。构建日志：[app-build.log](evidence/leader/2026-11-04/d9-device/app-build.log)。签名包留在本地 `build/d9-device-package/`，不将证书、私有配置或 HAP 纳入 Git。

测试前以旧包短暂启动挂载调试沙箱，仅用于备份；旧包截图/日志留在忽略目录 `build/d9-device-raw/`，不作为本轮新包通过证据。

## 2. A：#45 修复包与缩略图回归

两次新包启动均有 `LEOCAD_GL_RENDERER`：`passed=true, programs=9, skipped_programs=0, safe_render=false`。证据分别为 [首次启动日志](evidence/leader/2026-11-04/d9-device/manual-progress-session.hilog.txt) 与 [第二进程日志](evidence/leader/2026-11-04/d9-device/second-session-final.hilog.txt)。

组长按同一条指令完成三轮，全程不重启：经系统选择器打开 `下载/LeoCAD-D8/首发 建模闭环.ldr` → 确认可选择 → 打开 `Input/curated-device.mpd` → 选红杯 → Right / Ctrl+Z / Ctrl+Y → 点文件/子模型菜单；轮次间丢弃测试编辑。组长合并反馈“不卡，正常”，指令同时要求报告缩略图是否正常。

| 轮次 | 输入与结论 | 证据边界 |
|---|---|---|
| 1 | 组长手工，未报告卡住或缩略图异常 | 无该轮独立截图/坐标确认 |
| 2 | 组长手工，未报告卡住或缩略图异常 | 无该轮独立截图/坐标确认 |
| 3 | 组长手工，未报告卡住或缩略图异常；末态 MPD 可继续保存 | 末态截图红杯 UI `(-45,-10,0)`、标题 `*`，子模型缩略图非空 |

三轮均归属 PID 11449。因 hilog 环形缓冲与合并回复，未保留每轮完整起止时间、坐标与全部选择器回调；不能将末态截图冒充逐轮采样。最后一轮日志可见 22:50:06 LDR 和 22:50:31 MPD 的选择结果。证据：[末态截图](evidence/leader/2026-11-04/d9-device/manual-three-rounds-final.jpeg)、[手工阶段日志](evidence/leader/2026-11-04/d9-device/manual-three-rounds.hilog.txt)、[操作记录](evidence/leader/2026-11-04/d9-device/steps.txt)。

补充注入对照：零件面板隐藏/显示、所有零件→Brick→所有零件、搜索 `3003` 并清空、窗口恢复/最大化后均有实际缩略图。Brick 显示 156 项；搜索得到 2 项。所有零件界面曾显示 1163，后为 1161，这是运行时列表数量，不能作为包内官方库零件数量。界面样本：[面板隐藏](evidence/leader/2026-11-04/d9-device/parts-hidden.jpeg)、[恢复](evidence/leader/2026-11-04/d9-device/parts-shown.jpeg)、[Brick](evidence/leader/2026-11-04/d9-device/brick-thumbnails.jpeg)、[搜索](evidence/leader/2026-11-04/d9-device/search-3003.jpeg)、[切回所有零件](evidence/leader/2026-11-04/d9-device/all-parts-after-brick.jpeg)、[窗口恢复](evidence/leader/2026-11-04/d9-device/restored-window.jpeg)。滑动面板后应用仍响应，但未独立量化滚动距离或遍历全库。

**延迟日志限制**：22:50:32（PID 11449）和 22:56:18（PID 16935）各出现一次 `LEOCAD_THUMBNAIL_DEFERRED`，均为 `state 0 refs 0`，part 字段乱码。两份第一进程日志中的同时间记录是重复采集，不计为两次事件。随后界面可显示缩略图、操作及保存仍正常，但无法把乱码 part 与某一张最终图像一一对应。该记录不能独立证明生命周期根因已消除，也不能仅据乱码断言悬空指针。交 A/B 结合原始日志检查。

两次进程分别于 22:52:57.353、23:02:19.198 记录 `'main' ... returned 0`。本轮没有卡住，无需制造或混入 D8 旧线程栈。#45 当前结论为**失败序列三轮手工未复现，具备新包成功对照，仍保留根因/观测关联待核项**。

## 3. B / 组长：模型、系统文件与异常路径

| 项目 | 实测结果 | 证据 |
|---|---|---|
| MPD 中文路径另存 | 保存到 `下载/LeoCAD-D9/D9 子模型闭环.mpd`；标题无 `*` | [保存后](evidence/leader/2026-11-04/d9-device/mpd-saved.jpeg) |
| 正常退出→二次启动→重开 | 新 PID 16935 经系统选择器重开；红杯 `(-45,-10,0)`；main/child 均可查看 | [重开坐标](evidence/leader/2026-11-04/d9-device/mpd-reopened-coordinates.jpeg)、[child](evidence/leader/2026-11-04/d9-device/mpd-child.jpeg)、[main](evidence/leader/2026-11-04/d9-device/mpd-main.jpeg) |
| 文件结构读回 | 保存与重开文件逐字节相同；2 模型、3 条 type-1 引用；颜色 4/1/14、单位变换保留 | [保存文件](evidence/leader/2026-11-04/d9-device/mpd-saved.mpd)、[重开文件](evidence/leader/2026-11-04/d9-device/mpd-reopened.mpd)、[核对收据](evidence/leader/2026-11-04/d9-device/model-receipt.json) |
| 脏工程取消打开 | 再移动一次，UI `(-45,-20,0)`、标题 `*`；取消选择后原 MPD 身份/模型/脏状态保留 | [取消打开](evidence/leader/2026-11-04/d9-device/cancel-open-dirty-preserved.jpeg) |
| 脏工程取消另存为 | 同上，取消后保留 | [取消另存为](evidence/leader/2026-11-04/d9-device/cancel-saveas-dirty-preserved.jpeg) |
| 截断 LDR | 显示无效坐标错误，原 MPD 及 `*` 保留 | [错误提示](evidence/leader/2026-11-04/d9-device/truncated-rejected.jpeg) |
| 循环 MPD | 显示不支持循环子模型错误，原 MPD 及 `*` 保留 | [错误提示](evidence/leader/2026-11-04/d9-device/cycle-rejected.jpeg) |
| 零字节文件 | 显示无 LDraw 记录错误，原 MPD 及 `*` 保留 | [错误提示](evidence/leader/2026-11-04/d9-device/zero-byte-rejected.jpeg) |
| 合法空模型 | 有合法注释的空 LDR 可开、保存、重开；保存结果 61 bytes、无 type-1 引用 | [重开](evidence/leader/2026-11-04/d9-device/valid-empty-reopened.jpeg)、[保存文件](evidence/leader/2026-11-04/d9-device/empty-saved.ldr) |
| 系统保存目标冲突 | 将另存文件名设为已存在目录 `export-blocked.ldr`；系统拒绝并提示“已有同名文件夹，请重新输入。”；取消后原 LDR 与 `*` 保留 | [系统拒绝](evidence/leader/2026-11-04/d9-device/export-directory-conflict-result.jpeg)、[返回原工程](evidence/leader/2026-11-04/d9-device/export-rejected-dirty-preserved.jpeg) |

最后一项只覆盖**系统选择器的目标检查失败**，未进入应用 QFile 写失败路径。不能代替真实权限撤销、不可写目标、部分写入失败或恢复副本取回验收；#15 保持开放。本轮未断网、未验证断电原子替换，未将 D8 的离线成功记录计作 D9 新跑。

## 4. C：工具栏、字体、快捷键与布局

| 检查 | 结果与边界 | 证据 |
|---|---|---|
| 空 Time 工具栏 | 启动及布局恢复后无空白工具栏带；View→工具栏、右键菜单均无 Time。仍有“时间线”停靠面板，这是保留项 | [工具栏子菜单](evidence/leader/2026-11-04/d9-device/toolbars-submenu-no-time.jpeg)、[右键菜单](evidence/leader/2026-11-04/d9-device/toolbar-context-no-time.jpeg) |
| 菜单分隔线 | 所看 File/View/Help 无连续双分隔线，Help 无前导分隔线 | [File](evidence/leader/2026-11-04/d9-device/file-menu-separators.jpeg)、[View](evidence/leader/2026-11-04/d9-device/view-menu.jpeg)、[Help](evidence/leader/2026-11-04/d9-device/help-menu-separators.jpeg) |
| 裁剪快捷键 | 视口聚焦后 HDC 注入 Ctrl+F / Ctrl+P，无查找/打印对话框或命令界面出现 | [之前](evidence/leader/2026-11-04/d9-device/hidden-shortcuts-before.jpeg)、[Ctrl+F 后](evidence/leader/2026-11-04/d9-device/hidden-find-after.jpeg)、[Ctrl+P 后](evidence/leader/2026-11-04/d9-device/hidden-print-after.jpeg) |
| 字形与布局样本 | 本轮菜单、分类、颜色名与系统中文提示未见缺字方框；最大化/恢复窗口无所见菜单或面板重叠 | [恢复窗口](evidence/leader/2026-11-04/d9-device/restored-window.jpeg)、上述菜单与错误截图 |
| 完整矩阵 | 最小窗口、1280×800/1920×1080 两档显示设置、完整 Tab 焦点链未验；物理 Shift/Alt 右拖、滚轮、视角球中断未补测；基础渲染模式未重建重测 | 仍待后续，不将截图样本等同全部字体或输入矩阵通过 |

取消对话框后能继续菜单与模型操作，只证明这些样本未失去响应，不证明 Tab 焦点顺序完整正确。首发允许部分英文提示，沿用组长已确认的范围。

## 5. 原数据恢复与本地校验

两个新包进程均正常退出后，写回测试前备份的 `LeoCAD.conf`，读回 SHA-256 与备份一致：`66819ceff7026c86c6d3ccf088f3556068cc7bd78b7dc39202b161803fd10530`。沙箱原 `render-baseline.ldr` 与公共 `LeoCAD-D8` 的 7 个文件也全部哈希一致，共 9 项；[恢复收据](evidence/leader/2026-11-04/d9-device/restoration-receipt.json)。未把配置内容提交仓库。

新建 `LeoCAD-D9` 下测试副本、输出模型及冲突目录保留，原 D8 文件不变。设备保留 D9 新包、应用关闭；本轮未改 WLAN。

本轮本地 Python 静态检查：包内资源 5、网络范围 10、导出清单 8，共 **23 项通过、3 个命令 EXIT 0**；[原始输出](evidence/leader/2026-11-04/d9-device/static-regression.txt)。本轮未重跑全部桌面 QTest，既有 CI/宿主结果见 [D9 测试矩阵](D9_测试矩阵.md)与 [B D9 记录](MEMBER_B_D9.md)，不混算真机条目。

## 6. 当日出口与下一批交接

方案 D9 出口为“阻断缺陷均有负责人和截止时间”。以下是本轮本地交接安排，**截止为下一次处理/给出结论的目标时间，不是到点自动验收或关闭**；未写入远端 Issue，组员尚未逐项确认该安排。

| 项目 | 当前状态/下一步 | 负责人 | 目标截止（北京时间） |
|---|---|---|---|
| [#45](https://github.com/xiaotaop/leocad-harmony/issues/45) P1 / Alpha 阻断 | 新包三轮未复现；核对 `state 0 refs 0`、乱码 part 与加载生命周期，决定关闭依据；当前仍开放 | A（zycAlmira）牵头，B 协查，组长供设备 | D10 11/5 20:30 前给复核结论；需设备则进下一批 |
| [#15](https://github.com/xiaotaop/leocad-harmony/issues/15) 系统文件闭环 | 中文 MPD/取消/非法/空模型已补；实际拒绝授权、不可写与恢复体验仍待验证 | 组长 xiaotaop + B tynacity | D10 11/5 20:30 前准备可重复步骤，21:00 批次验证 |
| C 真机矩阵缺口 | 最小窗/指定显示档位、完整 Tab 链由 C 给操作判据，组长执行；无外接鼠标的物理组合项目明确未测 | C Leaf-21 + 组长；输入项 A 协助 | D10 11/5 20:30 前补步骤与可测条件 |
| [#16](https://github.com/xiaotaop/leocad-harmony/issues/16) 发布权限 | 仍含 Qt 默认 6 权限；正式签名前收敛，不能视为上架权限审核通过 | 组长，C 配合隐私/权限清单 | D10 11/5 20:30 前给权限清单处理结论 |
| [#49](https://github.com/xiaotaop/leocad-harmony/issues/49)、[#28](https://github.com/xiaotaop/leocad-harmony/issues/28)、[#14](https://github.com/xiaotaop/leocad-harmony/issues/14) P2 | 库错误引导、部分中文/省略、偏好设置裁剪继续跟进，不改写为 D9 新发现阻断 | 远端 #49 为组长，#28/#14 为 C；#49 请 C 协助 | D10 11/5 20:30 前确认处理顺序 |
| [#13](https://github.com/xiaotaop/leocad-harmony/issues/13) P2 生命周期 | 本轮两次正常退出无新失败，不等于覆盖全部 Ability 生命周期 | 组长 | D10 稳定性批次继续观察 |

D10 的 30 分钟稳定性和中型模型测试仍是下一日任务。本轮不提前冒认完成。全部证据文件的字节数/SHA-256 见 [manifest.json](evidence/leader/2026-11-04/d9-device/manifest.json)。
