# D8 组长 Alpha 集成与真机验收（2026-11-03）

## 当日结论

已将 A #46、B #47、C #48 合入后的主线构建为同一个正常模式 Alpha 包，完成 LDR 从零建模闭环、MPD 编辑后保存退出重开、异常文件和取消保护、断网文件闭环及界面资源核对。组长确认该包物理方向键、Ctrl+Z、Ctrl+Y 正常。

**核心样例闭环通过；完整 Alpha 放行不通过。** 同进程 LDR→MPD 复现 [#45](https://github.com/xiaotaop/leocad-harmony/issues/45) 的主窗口无响应，已取得四个时间点的应用原生线程栈。新进程 MPD 成功不能抵消失败轮；#45 保持开放，按本次实际影响作为 P1 阻断处理。功能范围按 [冻结通知](D8_ALPHA_FREEZE.md) 收口，不增加新页面或功能。

本次只改验收、证据、交接文档及证据字节保留规则，没有生产代码修改。组长随后要求将材料附到已有 #45，交付方式为推送证据开发分支并在该 Issue 添加固定提交链接；之后按组长要求创建草稿 PR，暂不合并，冻结通知仍为待团队确认的稿件。文档中的协作分工是交接建议，不代表已经修改 GitHub 指派。沿用组长要求：不录屏，允许部分英文提示。

## 唯一源码、包与设备

| 项目 | 实测值 |
|---|---|
| 源码 | `d9ff2a4f7b43f23773e5c4fdad91d00ac8674efe`，PR #48 合并后的 main |
| 本地工作分支 | `feature/leader-d8-alpha` |
| 应用 | `io.github.xiaotaop.leocad` / `QAbility` / 0.1.0 |
| 工具链 | Qt 6.12.0 ARM64；SDK 26.0.0.105；DevEco CLI 26.0.0.851 |
| 配置 | Release；`LEOCAD_SAFE_RENDER=OFF`；模型测试 HAP 开关 OFF；官方调试签名 |
| HAP | `build/d8-device-package/leocad-d8-alpha-signed.hap`；80,134,241 字节 |
| HAP SHA-256 | `d457e954696f56533a731034b1dd5c7fc6f488ae699a713e74ebe0ff423804fa` |
| native Build ID | `34d8ed0e87c6fc4d692c6742abfcfed9a53c8431`，本机构建与故障栈一致 |
| 设备参数接口 | HAD-W32；OpenHarmony-6.1.0.115；API 23；3120×2080 |
| 系统故障档案的 Build info | HAD-W24 6.1.0.117(SP68C00E100R13P3)；与参数接口分别照录，不据此混写系统版本 |
| GL | 9 programs / 0 skipped；正常模式；MPD 视图日志 DPR≈1.9，Maleoon 916 |

完整收据见 [build-receipt.json](evidence/leader/2026-11-03/d8-device/build-receipt.json)。包、证书、签名配置和原个人设置留在忽略目录。设备编号和设备指纹不进入提交。

复现构建：设置 `QT_ROOT` 为 `D:/qt/6.12.0/harmonyos_arm64_v8a`、`QT_HOST_PATH` 为同版本宿主 Qt，再执行：

```powershell
./scripts/build-harmony-app.ps1 -BuildDirectory build/harmony-d4 -GenerateSignature
```

安装的是上述复制并计算哈希的新 HAP，没有沿用 D7 包。生成包 `module.json` 中无 `ohos.permission.INTERNET`；这项静态检查与下文实际关闭 WLAN 的操作分别记录。

## 核心建模与文件结果

以下证据相对于 `docs/evidence/leader/2026-11-03/d8-device/`。除明确标注“组长物理确认”外，界面输入均为 HDC 注入，不能当作物理鼠标/组合键验证。

| 检查 | 结果、实际数据与证据 |
|---|---|
| 空模型→搜索→插入 | 通过。无命令行模型参数启动，包内库直接可用；搜索 `3003` 得到 2 项，从零件菜单插入 `3003.dat`；`alpha-first-start.jpeg`、`part-search-3003.jpeg`、`insert-command-result.jpeg` |
| 物理移动、撤销、重做 | 组长在新包亲手操作后回复“正常”。`physical-keyboard-result.jpeg` 保留最终状态；没有逐步采集三次坐标，不把确认解释为精确的单次位移测量 |
| 改色、旋转、删除与撤销 | 通过注入测试。选择颜色后执行“上色选定零件”，颜色 7→3；Shift+Right 改变旋转矩阵；Delete 后为空，Ctrl+Z 恢复。见 `painted-and-saved.jpeg`、`core-rotated.jpeg`、`core-delete.jpeg`、`core-delete-undone.jpeg` |
| LDR 新建、覆盖、正常退出、系统重开 | 通过。实际 DocumentViewPicker 保存“下载/LeoCAD-D8/首发 建模闭环.ldr”，后续 Ctrl+S 覆盖同一授权文件。PID 41365 正常退出，PID 44779 经系统选择器重开；无修改星号、颜色和旋转可见。`core-real-reopened.jpeg`；输出 `core-model-rotated.ldr` 与重开后读回相同 |
| LDR 最终结构 | 1 件 `3003.dat`、颜色 3、LDraw 平移 `(-150,-24,0)`；矩阵 `0.866025 -0.5 0 / 0.5 0.866025 0 / 0 0 1`。这是实际保存结果，不倒推用户每次物理按键的位移 |
| 同进程 LDR→MPD | **失败，#45。** PID 44779 的系统选择器返回 MPD 后出现主窗口不响应；没有把后续未生效的移动/撤销/重做记为通过。详见后节 |
| 新进程 MPD 编辑、撤销、重做、main/child 切换 | 通过独立对照轮。PID 46664 直接经系统选择器打开固定 MPD，红杯 UI 坐标 `(-45,0,0)→(-45,-10,0)`，撤销恢复、重做再次移动；main/child 均可显示。见 `mpd-direct-move.jpeg`、`mpd-direct-undo.jpeg`、`mpd-direct-redo.jpeg`、`mpd-child-view.jpeg` |
| MPD 编辑后保存→退出→系统重开 | 通过独立对照轮。另存“子模型 编辑闭环.mpd”，PID 46664 正常退出，PID 49150 从系统选择器重开，UI 红杯 `(-45,-10,0)`、无星号，child 显示正常。见 `mpd-edited-reopened-coordinates.jpeg`、`mpd-reopened-child.jpeg` |
| MPD 结构读回 | 2 模型 main/child，3 条引用及颜色 4/1/14 保留；红杯 LDraw 平移 `(-45,0,-10)`，child 引用 `(45,0,0)`，child 内零件 `(0,0,0)`，均为单位矩阵。`mpd-edited-saved.mpd` 与 `mpd-after-reopen.mpd` 字节相同 |
| 异常文件保护 | 通过。先将杯子再次移动到 UI `(-45,-20,0)`，保持 `*`，打开时选 No，再经系统选择器选循环 MPD/截断 LDR；均给出非空错误，原文件身份、main/child、场景与未保存状态保留。`cycle-rejected-dirty-mpd.jpeg`、`truncated-rejected-dirty-mpd.jpeg` |
| 取消打开、取消另存为 | 通过。脏 MPD 下分别取消系统选择器，文件身份、场景、坐标及 `*` 保留；`open-cancel-dirty-preserved.jpeg`、`saveas-cancel-dirty-preserved.jpeg`。之后撤销额外测试移动，未写回该脏状态 |
| 真实断网文件闭环 | 通过。控制中心关闭 WLAN，PID 51713 经系统选择器打开两砖 LDR、移动红砖、另存 `offline-roundtrip.ldr`、正常退出；PID 52739 经系统选择器重开。红砖 LDraw `(-30,0,-10)`，蓝砖颜色 1、原矩阵和平移不变；前后文件一致。`network-disabled.jpeg`、`offline-reopened-coordinate-proof.jpeg`、`network-still-disabled-after-roundtrip.jpeg` |

机器核对结果见 [model-and-restoration-checks.json](evidence/leader/2026-11-03/d8-device/model-and-restoration-checks.json)。系统打开/保存都保留实际选择器回调日志，不能以 Want 参数或沙箱读写替代。

## #45：失败轮取证与修复交接

失败轮包哈希、Build ID 均与上表一致，PID **44779**：

1. 21:11:32，系统选择器完成重开已编辑 LDR，模型显示正常。
2. 21:12:28，系统选择器返回 `Input/curated-device.mpd`，MPD 视图与 FBO 初始化日志仍正常。
3. 随后选择红杯、方向键、Ctrl+Z/Ctrl+Y、点击子模型菜单均无可见响应；21:12:33 起持续 watchdog。
4. 保留进程后，从 Hiview Faultlogger 导出 `20261103211237`、`20261103211349` 两份 appfreeze 档案。其中包含 21:12:33.882、21:12:36.877、21:13:39.887、21:13:48.855 四个原生全线程快照。
5. 四份快照的 **QtMainThread / TID 44859** 均停在：

```text
QThread::msleep
lcPiecesLibrary::LoadPieceInfo(PieceInfo*, bool, bool)
lcPiece::SetPieceInfo
lcPiece::lcPiece
lcModel::SetPreviewPieceInfo
lcThumbnailManager::DrawThumbnail
lcThumbnailManager::PartLoaded
QObject::event / QCoreApplication::sendPostedEvents
```

源码对应 `common/lc_library.cpp:1596` 的等待 Loaded 循环，调用方为缩略图预览创建。OHOS 主线程栈同时保留；不是凭相邻剪贴板日志归因。**已确认卡住的调用链，尚未确认哪一处零件状态/引用生命周期导致等待不能结束。** 不能据此认定 MPD 序列化或 Qt 文件授权本身是根因，也不能用超时强行跳过加载作为已验证修复。

21:15:51 取证完成后才 force-stop 失败进程，该轮无正常退出。PID 46664/49150 后续成功轮分别保存，不覆盖失败证据。

线程摘录：

- [21:12 档案](evidence/leader/2026-11-03/d8-device/appfreeze-io.github.xiaotaop.leocad-20020151-20261103211237.threads.txt)
- [21:13 档案](evidence/leader/2026-11-03/d8-device/appfreeze-io.github.xiaotaop.leocad-20020151-20261103211349.threads.txt)
- [失败进程 hilog](evidence/leader/2026-11-03/d8-device/mpd-unresponsive-final.hilog.txt)

完整系统档案保存在忽略目录 `build/d8-device-raw/`；提交摘录保留应用的全部原生线程快照和原件 SHA-256，去掉设备指纹及无关进程、系统数据。直接执行 dumpcatcher/processdump/SDK lldb-server 均受设备权限限制，实际成功方式是 `hidumper -s 1201 -a "-p Faultlogger -f <record>"`，限制见 `diagnostic-tool-limits.txt`。

修复建议分工：#45 现有负责人 A 继续牵头，C 协查缩略图、B 协查 PieceInfo 加载状态和引用生命周期；补缩略图/文件切换的宿主回归，修复包由组长复验同进程切换。A 要求的三轮纯手工切换、面板显示/隐藏及窗口变化对照没有全部执行；上述初次取证是注入操作复现并取得栈，不冒充纯手工覆盖。关闭 #45 必须以修复证据和真机复验为依据。

### 组长手工复现补充（21:49 起）

组长随后按复现步骤自行操作，并回复“已卡住”。使用此前留在真机上的 Alpha，未重装；失败进程为 **PID 59946**。助手在组长报告卡住后仅截图、读取日志和诊断信息，没有向应用注入点击/按键、重启或 force-stop。

- 21:49:53.298，系统选择器回调返回“首发 建模闭环.ldr”。
- 21:50:32.803，同一进程的选择器返回 `Input/curated-device.mpd`。
- 21:50:37.193 起持续 `QtWatchdog: AppMainThread is not alive`；组长确认交互卡住，截图仍显示 MPD 红杯/黄色底座。
- 原始应用日志、截图和时间线分别见 [手工日志](evidence/leader/2026-11-03/d8-device/manual-hang-pid59946.hilog.txt)、[现场截图](evidence/leader/2026-11-03/d8-device/manual-hang-pid59946.jpeg)、[手工轮收据](evidence/leader/2026-11-03/d8-device/manual-hang-receipt.json)。本轮未取得正常退出，采集结束时仍保留进程。
- **本轮未取得可导出的新原生线程栈**：查询 21:49 以后的 Hiview Faultlogger 返回无新档案，访问 `/data/log/faultlog/temp` 被拒绝。此前四份栈属于 PID 44779，不能写成 PID 59946 的栈，也不能断言两轮根因已被证明相同。

这补充证明了手工操作也能触发同类无响应，而非仅在 HDC 注入过程中出现；不代表完成了三轮手工切换或其他窗口条件矩阵。#45 的 P1 阻断结论保持不变。

## 界面、资源及范围边界

| 检查 | 结果 |
|---|---|
| 帮助菜单裁剪 | 通过。仅剩“关于…”，主页/报告问题入口消失，无空白项；`help-menu-alpha.jpeg` |
| 窗口变化与中文菜单 | 最大化/还原后仍可操作。最大化和还原两种宽度下，筛选菜单“正则表达式”“区分大小写”“零件描述”及选项中文无中间省略；`parts-filter-wide.jpeg`、`parts-filter-narrow-expanded.jpeg` |
| 字体与图标 | 本次观察到的主菜单、状态栏、面板选项、错误对话框中文无缺字方框；窗口/Dock 显示自定义红砖图标。没有重跑独立字体探针，也未另验桌面快捷方式/最近任务列表，不宣称全字符集或全图标场景通过 |
| 分类和空态 | 同进程“所有零件 1161→砖块 156→所有零件 1161”恢复正常，收藏夹 0 项时有中文空态。1161 是界面实际计数，不等同于清单中的 1160 个库零件；`brick-category-count.jpeg`、`all-parts-count-restored.jpeg` |
| 内置库 | 正常启动无需指定外部库，缩略图可见，没有库缺失警告。未破坏库来触发 #49；该错误提示改写另行跟踪 |
| 已接受限制 | `Part ID`、错误正文和标准按钮部分英文按组长既有决定允许；#32 相机适应范围仍按非阻断项保留 |

本地重新执行资源 5 项、网络范围 10 项、资源导出 8 项，共 **23 项通过**，命令和输出见 [static-regression.txt](evidence/leader/2026-11-03/d8-device/static-regression.txt)。没有将 Windows CI 的字体 SKIP 写成真机字体通过，也没有把宿主测试替代授权异常测试。

## 未测、恢复及出口

- **真实拒绝授权/无写权限、外部写失败恢复副本的用户可取回性未测**，#15 继续开放；正常保存、取消、坏文件及断网通过不足以关闭它。
- 曾对新建测试副本尝试 `chmod 444` 构造只读目标，系统返回 Permission denied，模式未变；未将其作为应用保存失败测试。具体命令、结果及清理见 `readonly-target-attempt.txt`。
- 真实 Shift/Alt 右拖、普通/高分辨率物理滚轮、棱线多点拾取和已激活视角球中断未测，不能从本次方向键确认或 HDC 注入推导。
- 缩放快捷键可发现性未另做用户测试；未加入菜单或新增页面。
- WLAN 已恢复开启；系统随后提示认证过期，组长自行完成认证并回复“网络已恢复”。不把“开关已打开”当成网络认证成功。
- 所有成功轮正常结束：41365、46664、49150、51713、52739、54836；失败 PID 44779、手工复现 PID 59946 分别记录，后者取证结束时仍保留。
- 原配置 **5420 字节**恢复后 SHA-256 为 `5575996fecc88dc99bb3513e4d8d5a68da11521b04685dfc3c6c65fa66a13631`，与操作前一致。沙箱原 LDR、原 MPD，以及公开输入副本均读回一致；原沙箱 MPD 为 381 字节，未用 250 字节测试输入覆盖它。
- “下载/LeoCAD-D8”保留本次输入副本与三个输出文件，原样例和个人数据未改写。证据校验见 `manifest.sha256.json`。

**D8 组长已完成本次构建、真机执行、恢复和证据整理；当日“Alpha 全面放行”出口仍被 #45 阻断。** 下一步先修 #45，并补 #15 的授权失败/恢复体验；继续功能冻结，不能把“已跑完验收”写成“全部验收通过”。
