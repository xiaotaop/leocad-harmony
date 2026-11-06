# D11 组长真机与安装验收（2026-11-06）

本轮**设备回归和安装生命周期验证通过，D11“RC0 与发布材料齐备”的统一出口尚未完成**。A 的修复已进入被测包；三轮切换后打开固定 2,000 实例 MPD 未复现 D10 崩溃。当前包仍为 debug 签名的内部 RC0 候选包，正式发布签名和控制台预填未完成，许可与包内声明等既有发布事项仍需闭环。

没有录屏。本文将用户手工反馈、HDC 注入和文件核对分别记录；本轮没有修改生产代码，也未创建远端 PR、Issue 或关闭既有问题。

## 1. 被测包与设备

完整身份见 [package-receipt.json](evidence/leader/2026-11-06/d11-device/package-receipt.json)。

| 项目 | 实测值 |
|---|---|
| 源码提交 | `96d14f760c9b904c2ae8db906e5ffdea27804a9d`，包含 A #60 的生命周期修复、B D11 回归及 C #62 |
| 包 | `leocad-d11-rc0-debug-signed.hap`，80,153,176 bytes |
| HAP SHA-256 | `67834801e859f7267b151901f4200a0111298d8e9a798150421fd6cbb22aa9f8` |
| 应用 ELF Build ID | `0fe58fb6eed6cd95e322b0b735dd7605ac80e037` |
| QtGui Build ID | `2954f3e83d766bba2564d823dc524f3fc225d9a5` |
| 编译 | Native CMake `Release`；Qt 6.12.0；`LEOCAD_SAFE_RENDER=OFF`；模型测试入口 OFF |
| HAP 内身份 | `io.github.xiaotaop.leocad` / `0.1.0` / versionCode `1` |
| 打包与签名 | 官方调试签名，HAP 实际 `debug=true`、`buildMode=debug`；不是商店 release 包 |
| 设备 model 参数 | `HAD-W32` |
| software.version 参数 | `HAD-W24 6.1.0.117(SP68C00E100R13P3)` |
| fullname 参数 | `OpenHarmony-6.1.0.115` |
| 显示及图形 | 3120×2080，活动模式 60 Hz；日志 DPR 约 1.9；HUAWEI Maleoon 916 / OpenGL ES 3.2 |

设备时间为 CST，与宿主 +08:00 一致；操作时间见 [steps.txt](evidence/leader/2026-11-06/d11-device/steps.txt)。原始包、匹配 ELF、完整构建日志及私有数据备份保留在本地忽略的 `build/`，不把调试证书或用户配置内容提交仓库。

## 2. 修复回归：切换后打开中型模型

### 第一会话：物理输入反馈及保存闭环

PID **39541**，正常渲染，“所有零件”分类。组长按请求用自带键盘/触控板执行三轮 LDR→MPD、选择、方向键、Ctrl+Z/Y 和菜单操作，回复“不卡住，正常”。[手工末态](evidence/leader/2026-11-06/d11-device/manual-three-rounds-final.jpeg)可见模型、缩略图及标题 `*`。

证据边界：[持续日志](evidence/leader/2026-11-06/d11-device/session-39541-live.hilog.txt)只有一组 `roundtrip.ldr`→`curated-device.mpd` 系统选择器回调；因此“三轮”作为用户反馈记录，不声称该日志独立证明了三轮选择器操作。下节另补可逐轮核对的自动重复。

21:19:55.551，经系统选择器打开与 D10 相同的固定 `d10-2000.mpd`，PID 不变，仍为“所有零件”。[模型与列表正常显示](evidence/leader/2026-11-06/d11-device/medium-open-confirmed.jpeg)，没有发生 D10 的图标绘制 SIGSEGV。HDC 后续验证：

- 搜索旧 `child.ldr` 返回 0；[空搜索结果](evidence/leader/2026-11-06/d11-device/old-child-search.jpeg)保留。清空搜索，主模型下[四个新子模型实际缩略图](evidence/leader/2026-11-06/d11-device/four-submodel-thumbnails.jpeg)可见。
- 切到 child-0，在俯视图选中第 222 件 `3032.dat`，初始 UI `(50,550,0)`。注入方向键后为 `(60,550,0)`，撤销恢复、重做再次移动，随后撤销该位移；通过属性将 Y 设为 540，并将该件颜色 12 改为 4。改色撤销、重做各留图；[最终属性](evidence/leader/2026-11-06/d11-device/child-color-redo.jpeg)为 `(50,540,0)`、红色。本次不声称方向键直接产生了最终 Y 位移。
- [零件面板隐藏](evidence/leader/2026-11-06/d11-device/parts-hidden.jpeg)及[恢复](evidence/leader/2026-11-06/d11-device/parts-restored.jpeg)、[窗口还原](evidence/leader/2026-11-06/d11-device/window-restored.jpeg)及[再次最大化](evidence/leader/2026-11-06/d11-device/window-maximized-again.jpeg)后继续响应。child-0 活动时“子模型”显示 3 个同级模型，与主模型下显示 4 个的口径不同。
- 系统另存为 `下载/LeoCAD-D11/Input/D11 中型模型闭环.mpd`，[中文路径保存后](evidence/leader/2026-11-06/d11-device/medium-saved.jpeg)标题无 `*`。21:26:47.376 正常退出，`main returned 0`。

### 新进程直接打开对照与重开

同一包，PID **44332**：21:27:53.799 通过系统选择器直接打开原固定中型模型，[成功显示](evidence/leader/2026-11-06/d11-device/fresh-control-medium.jpeg)。随后打开上述中文保存文件，切回 child-0 并选择编辑件，[重开属性](evidence/leader/2026-11-06/d11-device/reopened-edit-properties.jpeg)仍为 `(50,540,0)`、红色。21:29:07.551 正常返回 0。

独立解析原始输入，预先按已记录编辑生成期望结构，再与[保存文件](evidence/leader/2026-11-06/d11-device/medium-saved.mpd)和[重开后读取文件](evidence/leader/2026-11-06/d11-device/medium-reopened.mpd)比较：5 模型，主模型 4 条引用，每个子模型 500 件；顺序、编号、颜色及全部位姿与期望完全一致，其余模型无变化。两份输出均为 **85,317 bytes**，SHA-256 均为 `9518248df9bfbcab75df382157cfe22c30fffcdf1c71632d80d495ea0c34f253`。详见 [model-receipt.json](evidence/leader/2026-11-06/d11-device/model-receipt.json)。本次仅改一件颜色，不沿用 D10“全部 500 件改红”的期望。

### 补充会话：三轮系统选择器回调完整留证

PID **49060**，同一包、正常模式、最大化窗口、“所有零件”。本节全部为 HDC 注入，不代替物理键盘结论。每轮经选择器打开 LDR→MPD，选红杯，Right→Undo→Redo，再开文件/子模型菜单；下一轮对测试模型选“不保存”。

| 轮次 | LDR 回调 | MPD 回调 | 编辑结果与缩略图 |
|---|---|---|---|
| 1 | 21:36:19.363 | 21:36:23.320 | [移动](evidence/leader/2026-11-06/d11-device/injected-r1-move.jpeg)、[撤销](evidence/leader/2026-11-06/d11-device/injected-r1-undo.jpeg)、[重做](evidence/leader/2026-11-06/d11-device/injected-r1-redo.jpeg) |
| 2 | 21:37:53.703 | 21:37:57.737 | [移动](evidence/leader/2026-11-06/d11-device/injected-r2-move.jpeg)、[撤销](evidence/leader/2026-11-06/d11-device/injected-r2-undo.jpeg)、[重做](evidence/leader/2026-11-06/d11-device/injected-r2-redo.jpeg) |
| 3 | 21:38:50.727 | 21:38:54.747 | [移动](evidence/leader/2026-11-06/d11-device/injected-r3-move.jpeg)、[撤销](evidence/leader/2026-11-06/d11-device/injected-r3-undo.jpeg)、[重做](evidence/leader/2026-11-06/d11-device/injected-r3-redo.jpeg) |

三轮都可选红杯，UI 初始 `(-45,0,0)`，移动/重做 `(-45,-10,0)`，撤销恢复；缩略图和菜单可见。21:40:05.080，同一 PID 经选择器打开原 2,000 实例模型，[成功显示](evidence/leader/2026-11-06/d11-device/injected-three-rounds-medium.jpeg)，再切“子模型”显示[四个实际缩略图](evidence/leader/2026-11-06/d11-device/injected-three-rounds-four-thumbnails.jpeg)。21:40:51.868 正常返回 0。[完整进程日志](evidence/leader/2026-11-06/d11-device/injected-three-rounds-final.hilog.txt)包含七次选择器回调。

## 3. 安装、覆盖安装与卸载重装

1. 从 D10 已安装状态覆盖安装本次 HAP，成功启动 PID 39541。卸载前再读取 7 份非缓存私有文件，其哈希均与本轮开始备份相同；这证明这些模型/资源未被覆盖安装或回归操作改写，不代表全部系统状态不变。
2. 正常退出后卸载，HDC 返回 [uninstall bundle successfully](evidence/leader/2026-11-06/d11-device/uninstall-result.txt)。随后 `bm dump` 仅返回一般查询失败，不能将该输出单独解释成明确的“不存在”错误码。
3. 安装同一 HAP，返回 [install bundle successfully](evidence/leader/2026-11-06/d11-device/clean-install-result.txt)。PID **46950** [首次启动](evidence/leader/2026-11-06/d11-device/clean-install-first-start.jpeg)为空模型、默认布局且图标可见；配置为新生成的 29 bytes，旧私有 `a-d3-device.ldr` 不存在。卸载清理私有数据符合预期。
4. 公共下载目录仍保留测试文件。通过新的系统选择器选择中文保存模型，[重新打开成功](evidence/leader/2026-11-06/d11-device/clean-install-public-saved-open.jpeg)；21:31:47.189 正常退出。
5. 恢复 **7 份非缓存私有文件 + 4 份配置**，逐份重新读取并核对原备份 SHA-256，11/11 一致，见 [恢复收据](evidence/leader/2026-11-06/d11-device/restored-private-files.json)。PID **48688** 用恢复后的配置[启动正常](evidence/leader/2026-11-06/d11-device/restored-original-config-start.jpeg)，21:34:06.082 返回 0。
6. 补充自动会话结束后再次恢复原配置并核对；另核对 [8 份原 D10 公共文件](evidence/leader/2026-11-06/d11-device/public-d10-preserved.json)，均与本轮开始时的备份哈希一致。设备保留 D11 新包和公共测试输出，应用停止。未改 WLAN。

备份范围：完整 base 目录复制曾有缓存权限错误及旧嵌套路径不存在的警告，**不宣称全沙箱备份成功**。可读原模型、测试资源和配置单独备份并核对；运行时 shader/font 缓存、hiappevent 日志数据库不回灌。备份内容只留本地，不提交配置正文。

## 4. 图形日志、宿主检查和边界

[日志汇总](evidence/leader/2026-11-06/d11-device/log-summary.json)按进程分别统计，持续日志与最终快照可能重复，不能相加当事件总量。五个进程的 renderer 均 `passed=true, programs=9, skipped_programs=0`；记录的 framebuffer 均通过。所采日志无 `LEOCAD_THUMBNAIL_DEFERRED`、SIGSEGV/SIGABRT 或 APP_FREEZE 命中；五个进程均有正常返回 0。本轮未观察到卡死或持续空缩略图。

每个进程启动约 10 秒后仍有一组 QtWatchdog 警告：`AppMainThread is not alive`，随后 `Thread may be blocked, do not report this time`，`lastTime: 0.000`。警告之后仍能响应操作并正常退出，不能等同于已确认卡死；也不能写成“无 watchdog 警告”。这些原始行保留供 A/平台侧分析。本轮结果支持修复回归成功，不证明所有会话历史和任意模型均无崩溃，也未新增 30 分钟压力结论。#45 没有自动关闭。

[宿主检查输出](evidence/leader/2026-11-06/d11-device/host-checks.json)：发布材料 4、权限 5、资源 5、网络范围 13、零件清单 8、截图样例 6，合计 **41 项通过**。POSIX 日志留存套件在此 Windows Python 环境执行失败：系统 `bash` 为 WSL 入口，子进程报编码错误；尝试在 PATH 中优先 Git Bash 后[重跑仍失败](evidence/leader/2026-11-06/d11-device/host-log-retention-retry.txt)，不能据此断定子进程实际切换成功，也不能列为通过。本轮不修改该独立宿主适配问题，保留失败输出；不把 A/B/C 的异机测试冒充本机已执行。

实际 [HAP module.json](evidence/leader/2026-11-06/d11-device/packaged-module.json)仍声明 6 项 Qt 默认权限，没有 INTERNET。#16 权限收敛未完成；#15 的真实权限拒写、QFile 写失败和恢复副本取回本轮未测，选择器重新选择或卸载后的数据恢复不代替这些场景。

## 5. D11 交接与未完成项

- 设备侧：本轮固定失败序列回归、直接打开对照、中文保存重开和安装生命周期可以按本文验收；证据随组长工作分支保存，问题是否关闭另行判断。
- 发布包：当前 Native Release + debug HAP。正式发布证书/Profile、release 打包及最终分发产物核验仍待完成，不能把该候选 HAP 上传成正式发布包。
- 控制台：已启动浏览器；电脑操作工具因不能可靠识别当前浏览器 URL 而停止操作。**未打开到提交表单、未预填、未保存云端草稿、未提交审核。** 已整理[本地预填表与待补项](D11_STORE_PREFILL.md)，它不等于云端完成。
- 材料：C 的冻结材料已集成，但其 [D11 交接表](MEMBER_C_D11.md)中的许可兼容性、商标/内容分发、LDraw 声明、包内许可证呈现、源码发布标识等尚未得到本轮闭环；RC1 六张商店截图按 D12 计划拍摄。

因此，本轮设备工作完成，但**不能宣布整个小组 D11 发布出口已经通过，或已经可以提交正式上架审核**。
