# D10 组长真机回归（2026-11-05）

## 当前结论

**发现新阻断，不能据本轮宣布 Alpha 或 2,000 实例规模验收通过。** 新包 PID 32382 在组长三轮手工 LDR→MPD 后，经系统文件选择器打开 `d10-2000.mpd`，发生 QtMainThread SIGSEGV。新进程 PID 35069 直接打开同一文件成功，后续对照结果见下文。成功对照不能抵消已发生的崩溃。

本文记录组长设备执行，不将成员 A/B/C 的宿主测试当作真机通过。未录屏，未改生产代码，未关闭 Issue、创建远端问题或 PR。

## 1. 包身份与设备

- 源码：本轮从已集成 A #57、B #58、C #56 的主线构建，完整 SHA 见 [构建收据](evidence/leader/2026-11-05/d10-device/build-receipt.json)；被测源码与本文档提交分开记录。
- `leocad-d10-signed.hap`：80,154,238 bytes；SHA-256 `de2faff13a08f75eca80c39fe2995b4b05ef45bbccb7dfd3355c01457d8b1243`。
- Native Build ID：`6de8cbbe920edd63e23b21ce325306508de93338`。
- Qt 6.12.0、SDK 26.0.0.105；Release；`LEOCAD_SAFE_RENDER=OFF`、`LEOCAD_BUILD_MODEL_TEST_HAP=OFF`。本轮未用基础模式替代正常模式。
- 设备参数 model 为 HAD-W32，软件版本为 `HAD-W24 6.1.0.117(SP68C00E100R13P3)`，fullname 为 `OpenHarmony-6.1.0.115`；崩溃报告也记录该软件版本。这几个不同字段按原值保留，不混写为同一个型号/版本字段。
- 实际显示 3120×2080；日志 DPR 约 1.9；GPU HUAWEI Maleoon 916 / OpenGL ES 3.2。
- 构建、签名、覆盖安装成功；两次启动均有 `LEOCAD_GL_RENDERER passed=true, programs=9, skipped_programs=0`。启动命令到截图是粗粒度观察，不作为点击图标到首帧/可交互的精确计时。

## 2. 第一进程：手工切换成功，打开中型模型时崩溃

PID 32382 于约 20:00:18 完成渲染初始化。组长用自带键盘/触控板连续三轮，全程不重启：打开 `下载/LeoCAD-D10/Input/roundtrip.ldr` → 确认可选择 → 打开 `curated-device.mpd` → 红杯 Right / Ctrl+Z / Ctrl+Y → 文件/子模型菜单。组长回复“正常”。第三轮末态红杯 UI `(-45,-10,0)`、标题 `*`，缩略图可见；[末态截图](evidence/leader/2026-11-05/d10-device/manual-three-rounds-final.jpeg)。未要求逐轮单独回复，不将末态截图冒充三轮独立坐标取证。

本轮持续采集 [第一进程 hilog](evidence/leader/2026-11-05/d10-device/live-session.hilog.txt)。20:03:09.984 出现一次 `LEOCAD_THUMBNAIL_DEFERRED`，part 乱码、`state 0 refs 0`。此后界面继续响应。

20:05:05.234，系统选择器返回 `Input/d10-2000.mpd`；.282 新视口初始化及 framebuffer 检查通过，.308 QtMainThread 在地址 `0x8` 发生 SIGSEGV。进程消失，桌面截图见 [失败现场](evidence/leader/2026-11-05/d10-device/medium-open-confirmed.jpeg)。这不是正常退出或 CLI 存活检查误判。

系统故障档案 `02263796989034418629` 已通过 `hidumper -e --print` 导出：[完整崩溃栈](evidence/leader/2026-11-05/d10-device/medium-crash-32382.txt)。主要路径为：

```text
QtMainThread / SIGSEGV(SEGV_MAPERR) @ 0x8
libQt6Gui.so 未符号化的顶部帧
QPixmapIconEngine::scaledPixmap
QPixmapIconEngine::paint / QIcon::paint
QStyledItemDelegate::paint
QListView::paintEvent
```

进程 RSS 360,349 kB、设备可用内存约 23 GiB，没有内存耗尽证据。栈尚未识别具体列表、行、pixmap 或 PieceInfo。乱码延迟事件比崩溃早约 115 秒，缺少对象地址和代次，不能认定二者同根因。

原 #45 是缩略图路径同步等待 Loaded 的挂起，本次是图标缩放绘制崩溃；两种失败形态分开记录。当前不能断言由 A 的取消加载改动引入，也不能宣布 #45 根因已消除。

## 3. 第二进程：中型模型成功对照

新进程 PID 35069，不重装包，20:06:36.203 经相同系统选择器直接打开同一 MPD，成功显示。固定样例为主模型引用 4 个子模型，每子模型 500 个实例，共 2,000 实例、16 种网格；不是完整 LDraw 库或任意 2,000 件模型的性能保证。

已执行：主模型和 child-0 切换；右拖旋转、Shift+右拖平移、滚轮缩放（均为 HDC 注入）；选择 child-0 中 Plate 4×6 / 3032，移动、撤销、重做；将颜色改为红色后撤销/重做。预期保存目标：该条目从输入的 LDraw `(50,0,550)`、颜色 12，变为 UI `(50,540,0)`，对应 LDraw `(50,0,540)`，颜色 4、单位旋转矩阵。其余条目应与固定输入一致。

界面截图：[原始属性](evidence/leader/2026-11-05/d10-device/child-properties.jpeg)、[改色重做](evidence/leader/2026-11-05/d10-device/child-color-redo.jpeg)。搜索、窗口恢复/最大化、零件面板隐藏/恢复、最小化后经 `aa start` 恢复均未换 PID，画面继续响应。脏工程取消打开、取消另存为后身份、活动子模型和标题 `*` 保留。组合操作属于注入样本，不冒充完整物理鼠标矩阵。

### 30 分钟对照结果

20:07:07.955 至 20:37:13.052，同一 PID 35069，累计至少 30 分钟。每分钟采集 `hidumper --mem`，共 31 份；每 5 分钟注入一轮旋转、平移、滚轮缩放、Right / Undo / Redo / Undo，并截图，七个检查点画面均可更新。主模型与 child-0 分阶段切换，约第 18 分钟切回完整 2,000 实例主场景。不是 30 分钟内持续不间断输入，也不是每个检查点都证明每条命令生效。

约第 10 分钟布局操作后选中状态丢失，该检查点的方向键不能算作有效零件编辑；随后重新全选 child-0 的 500 件，以及主模型的 4 条引用继续对照。20:32 的改色尝试与定时输入相邻，后续属性仍显示多个颜色，不作为最终改色成功证据；操作记录保留原条目并追加纠正。20:37:13 采样结束后重新设红，[最终截图](evidence/leader/2026-11-05/d10-device/final-child500-red.jpeg)显示选中 500 件且颜色为红色，保存结构也独立确认全部为颜色 4。注入脚本与当时 helper 留档于证据目录（记录执行方法，路径相对本地 build，不承诺直接从证据目录启动即可复现）。

| 分钟 | PSS kB | SwapPSS kB | 合计 MiB |
|---:|---:|---:|---:|
| 0 | 305472 | 0 | 298.3 |
| 5 | 435294 | 0 | 425.1 |
| 10 | 438274 | 23004 | 450.5 |
| 15 | 408751 | 19980 | 418.7 |
| 20 | 436105 | 17484 | 443.0 |
| 25 | 443056 | 16700 | 449.0 |
| 30 | 434010 | 16692 | 440.1 |

完整样本：[memory-samples.csv](evidence/leader/2026-11-05/d10-device/memory-samples.csv)。初期加载/窗口/模型状态变化伴随增长，后续检查点波动，没有单调持续上涨；这不足以证明没有泄漏或任意长会话都稳定。未测 FPS/精确首帧/逐操作超过一秒比例，不把这些项目列为通过。

### 中文保存重开与异常输入

最终将 child-0 的全部 500 件改为红色（颜色 4），保留原 3032 的单次位置差值 `(50,0,550)` → `(50,0,540)`。保存至 `下载/LeoCAD-D10/D10 中型模型闭环.mpd`，正常退出 PID 35069；新 PID 46458 经系统选择器重开，标题无 `*`，child-0 可全选 500 件且属性显示红色。保存/重开两份文件均为 85,120 bytes，逐字节一致。将保存结果与**由固定输入及上述预期编辑生成的期望结构**比较，5 模型、4 引用、每子模型 500 件均匹配；主模型与其余三个子模型完全不变，child-0 仅预期颜色和一条位移变化。见 [模型核对收据](evidence/leader/2026-11-05/d10-device/model-receipt.json)、[重开界面](evidence/leader/2026-11-05/d10-device/medium-reopened-child500.jpeg)。

采样期间脏中型工程拒绝循环 MPD、截断 LDR，保留身份与 `*`。第三进程再确认零字节文件被拒绝、原已保存模型保留；合法注释空模型可打开、保存、重开，输出 62 bytes，无 type-1 引用。证据：[循环拒绝](evidence/leader/2026-11-05/d10-device/medium-cycle-rejected.jpeg)、[截断拒绝](evidence/leader/2026-11-05/d10-device/medium-truncated-rejected.jpeg)、[零字节拒绝](evidence/leader/2026-11-05/d10-device/zero-byte-rejected-confirmed.jpeg)、[合法空模型重开](evidence/leader/2026-11-05/d10-device/legal-empty-reopened.jpeg)。

PID 35069 于 20:39:18.397、46458 于 20:42:24.990 正常返回 0。第一进程的 SIGSEGV 独立保留，不能用这两次正常退出覆盖。

## 4. 权限产物与未测项目

本轮实际解包的 [packaged-module.json](evidence/leader/2026-11-05/d10-device/packaged-module.json) 仍声明 6 项：STORE_PERSISTENT_DATA、FILE_ACCESS_PERSIST、PREPARE_APP_TERMINATE、READ_PASTEBOARD、GET_FILE_ICON、PRINT；没有 INTERNET。READ_PASTEBOARD 带模板 reason 和 `when: always`。另保留生成前工程清单作为对照，不能用生成文件代替 HAP 内实际清单。

未在本轮修改默认权限；#16 保持发布前收敛。应用内没有直接使用某权限，不足以证明 Qt 后端不需要，也不能证明拒权后的全部行为。#15 的可证实权限撤销、不可写目标、QFile 写失败和恢复副本取回仍须独立验证；不以选择器取消或同名目录冲突替代。

本轮未获得准确首帧/帧时中位数/慢帧数据，不将 HDC 命令耗时写成 GPU 帧时；未声称测试了指定的两档显示设置。

## 5. 交接

- A 牵头复核本次图标绘制崩溃，优先用匹配的 QtGui ELF 符号化顶部帧，补具体列表/行/缩略图 ID/pixmap DPR/cacheKey 与对象代次。诊断名称须在对象有效时复制。
- B 配合固定包与文件 SHA，比较“新进程直接打开”和“三轮切换后打开”，逐个改变会话历史、面板类别或打开入口，核对模型切换和临时模型回收边界。
- 组长保留新包、失败及成功对照材料，下一包修复后重跑失败序列；#45 不关闭，#15/#16 的未测项独立保留。
- 目标为 D11 11/6 20:30 前给出分析或修复包交接，21:00 批次复验；这是本地安排建议，未冒充组员已接受或远端已分派。

## 6. 恢复与交付校验

应用退出后恢复测试前 `LeoCAD.conf`，读回哈希一致。公共 D9 的 7 个原文件及沙箱基准模型也与备份逐项哈希一致，共 9 项；[恢复收据](evidence/leader/2026-11-05/d10-device/restoration-receipt.json)。私有配置、证书和 HAP 不进 Git。设备保留 D10 新包及 D10 测试副本/输出，应用关闭，WLAN 未改。

本轮本地静态检查：权限 5、网络 10、资源 5，共 20 项通过；原始输出在证据目录。本轮没有重跑全部宿主 QTest，成员 CI 结果与真机结果不混算。构建/签名日志、实际 HAP 模块清单、崩溃栈、三个进程日志、模型文件、31 份内存样本及截图均附 [manifest.json](evidence/leader/2026-11-05/d10-device/manifest.json) 大小/SHA-256 清单。

**组长本轮构建、设备执行、证据整理及发布限制更新已完成；整体验收不通过。** 新崩溃待分析/修复，#45、#15、#16 均不关闭。未为凑满任务修改生产实现，也未将 #15 的真实拒权/写失败恢复、精确帧时或完整输入矩阵冒认为通过。下一包需要复验上述失败序列，30 分钟成功对照不能替代该复验。
