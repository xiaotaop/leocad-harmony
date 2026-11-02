# D7 组长真机记录（2026-11-02）

## 结论与范围

组长 D7 主线“系统文件选择/沙箱、窗口操作、快捷键集成”已执行，修复了系统选择器授权的外部文件无法保存的问题。输入、撤销重做、正常渲染已进入同一集成包。**本记录不是全组全部交接项、D8 Alpha 或上架验收全部通过的声明**：物理鼠标组合键/滚轮、拒绝授权写入、断网及完整 MPD 编辑闭环仍有未测项，见后表。

仅使用组长真机；按组长要求不录屏。代码和证据保留在本地 `feature/leader-d7-device`，本轮未推送、创建 PR 或发布 GitHub 评论。

## 源码、包和设备

基线为 `main c52eb4ebcb8120b3c06e34e8d95118fd07fac4f3`，已包含 A #40、B #42、C #41/#43。保存修复源码为 `ba4bc21f7ce7e0e6a578ce0990e85feb21925c0c`；最终包在该提交前从相同工作区源码构建，提交后未更改生产代码。

| 项目 | 实测值 |
|---|---|
| 真机参数 | HAD-W32；`OpenHarmony-6.1.0.115`；API 23；屏幕 3120×2080 |
| 工具链 | Qt 6.12.0 ARM64；SDK 26.0.0.105；DevEco CLI 26.0.0.851 |
| 应用 | `io.github.xiaotaop.leocad` / `QAbility` / 0.1.0 |
| 构建 | CMake Release；`LEOCAD_SAFE_RENDER=OFF`；模型测试 HAP 开关 OFF；官方调试签名 |
| 最终 HAP | `build/d7-device-package/leocad-d7-final-signed.hap`；80,141,982 字节 |
| 最终 SHA-256 | `48375f884f6075cbbffec5e94506be5693911952c3b29107b066f271e688d571` |
| 最终两轮进程 | PID 35098、37873；9 programs / 0 skipped；native main 均返回 0 |
| 物理输入 | 组长确认：电脑自带键盘和触控板；没有提供物理鼠标/滚轮验收结果 |

完整机器可读信息见 [build-receipt.json](evidence/leader/2026-11-02/d7-device/build-receipt.json)。包、签名文件和原配置备份均留在忽略目录，不提交证书、密钥、设备唯一标识或完整个人配置。

本批次分三包记录，不能混用结论：

1. 基线 `c52eb4eb` 包 `157dd920…261ef`：系统打开/取消、**用户物理**方向键/Ctrl+Z/Ctrl+Y、保存失败现场。
2. 中间修复包 `206f91ec…d3804`：未提交的探索性构建，尚未加入保存错误中文译文；MPD、窗口和菜单复测属于此包，不冒充最终包。与最终包的保存流程一致，随后仅完善错误文案/翻译。
3. 最终 `ba4bc21f` 源码包 `48375f88…8d571`：中文路径新建保存、覆盖、正常退出重开，以及异常文件/取消保护。

构建命令：设置 `QT_ROOT` 为 Qt Harmony ARM64、`QT_HOST_PATH` 为同版本宿主 Qt，执行 `scripts/build-harmony-app.ps1 -BuildDirectory build/harmony-d4 -GenerateSignature`。安装必须使用收据中对应包，不能沿用旧 HAP。

## 文件选择与保存修复

实际调用 Qt OHOS 的 `DocumentViewPicker.select()/save()`。公开目录样例由系统界面选取，回调为 `file://docs/storage/Users/currentUser/Download/...`；Qt 本地路径为 `/storage/Users/currentUser/Download/...`。启动时的 Want/沙箱加载单独记录，**不计为系统选择器通过**。

基线包选择“中文 目录/模型 roundtrip.ldr”后报 `Operation not permitted`，已有外部文件 Ctrl+S 同样失败。原因是 `QSaveFile` 要求创建同目录临时文件并替换，而选择器授权只覆盖所选文件。

本次修复先完整序列化模型；仅 Harmony 外部文档路径改由 `QFile` 平台文件引擎写入，已有文件先在私有缓存创建恢复备份。写入后读回逐字节核对，成功后才更新文档身份和保存点；常规失败尝试恢复旧内容，恢复失败则保留备份并提示路径。沙箱与桌面继续使用 `QSaveFile` 原子替换，不新增广泛存储权限。

**边界：外部授权文件采用读回校验和失败恢复，不具备断电/进程被杀时的原子替换保证。** 缓存副本不是用户可管理的长期备份，相关异常存储与恢复体验需继续纳入 D8–D10 验收。

| 检查 | 结果与证据 |
|---|---|
| 系统打开、取消 | 基线包真实 DocumentViewPicker；取消回调为空，原文档保留；日志 `baseline-before-fix.hilog.txt` |
| 物理键盘移动/撤销/重做 | 组长亲手操作并明确回复“撤销恢复、重做再次移动均正常”；红砖 UI 坐标 `(-30,0,0)→(-30,-10,0)`，标题有 `*`；[前](evidence/leader/2026-11-02/d7-device/physical-keyboard-start.jpeg)、[后](evidence/leader/2026-11-02/d7-device/physical-keyboard-after.jpeg) |
| 首次中文路径保存 | 中间包成功保存“中文 目录/模型 roundtrip.ldr”，覆盖后读回红砖 LDraw 坐标 `(-30,0,-10)`；新进程系统重开 UI 为 `(-30,-10,0)` |
| 最终包新建/覆盖 | 保存“中文 目录/最终 验收.ldr”，再注入方向键并 Ctrl+S；外部读回见 [final-external-ldr.ldr](evidence/leader/2026-11-02/d7-device/final-external-ldr.ldr)，红砖 LDraw `(-30,0,-20)`，蓝砖颜色和矩阵不变 |
| 最终包退出重开 | PID 35098 正常退出；PID 37873 从系统选择器重开，标题无 `*`、UI 红砖 `(-30,-20,0)`；[截图](evidence/leader/2026-11-02/d7-device/final-reopened-coordinates.jpeg)；两轮均正常退出 |
| MPD 无编辑保存/重开 | 中间包通过系统打开、另存到“中文 目录/curated-device.mpd”、读回、同进程再次系统打开；保留 main/child、3 条引用、颜色 4/1/14、平移及单位矩阵；[输出](evidence/leader/2026-11-02/d7-device/external-mpd-after-save.mpd)、[子模型列表](evidence/leader/2026-11-02/d7-device/mpd-reopened-submodels.jpeg) |
| MPD 编辑撤销重做 | 中间包注入移动、Ctrl+Z/Ctrl+Y，红杯 UI `(-45,0,0)↔(-45,-10,0)`；不是物理键盘复测；最终撤销后丢弃测试状态。尚未完成编辑后另存另一 MPD、退出再开完整链 |
| 截断文件 | 最终包拒绝 `truncated.ldr`，原模型保留；[截图](evidence/leader/2026-11-02/d7-device/truncated-file-rejected.jpeg) |
| 循环引用与未保存状态 | 最终包先移动红砖，打开询问选 No，再选 `cycle.mpd`；拒绝加载后原文件身份、两砖、坐标及 `*` 保留；[截图](evidence/leader/2026-11-02/d7-device/cycle-rejected-dirty-preserved.jpeg) |
| 取消另存为 | 最终包系统保存界面点取消，文件身份、两砖、已编辑坐标和 `*` 保留；[截图](evidence/leader/2026-11-02/d7-device/save-cancel-dirty-preserved.jpeg) |
| 实际拒绝授权/无写权限 | 未测；宿主失败回滚测试不替代真机授权失败结论 |
| 断网打开/保存 | 未切换系统网络，本轮未测；包中没有 INTERNET 声明及网络源码检查通过，不替代断网实测 |

本轮自动操作使用 HDC 注入。除明确标“物理”的一行外，不作为物理组合键验收。

## 窗口、中文和字体

中间包完成最大化→还原、最小化→Dock 恢复、拖动窗口边界缩小、再次最大化。窄窗系统窗口矩形为 `[775,0,1276,1132]`（物理像素），截图保留在 `window-*.jpeg`。最大化后实体内部拾取与移动/撤销可用；**DPR 数值、边线拾取多点矩阵、导航中失活/最小化取消及物理滚轮没有重新验收**。既有 Front 取景裁剪 #32 仍按已接受的非阻断项处理，本轮不宣布其已修复。

零件选项与筛选子菜单在窄窗显示“编辑零件库”“显示零件名称”“正则表达式”“区分大小写”“零件描述”，未见这些词中间省略，见 [窄窗菜单](evidence/leader/2026-11-02/d7-device/narrow-filter-menu.jpeg)。`Part ID` 仍为英文，异常文件错误正文、Qt Yes/No/Cancel/OK 按钮也仍有英文；不能据此声称全界面中文完成。Brick→All Parts 同进程计数往返本轮未补测。

实际运行 C 的 `tools/d7/tst_d7_fonts.cpp` 独立诊断 HAP：Qt 6.12.0、平台 ohos，255 个字体族中 58 个支持简体中文，默认解析 `HarmonyOS Sans`，探针所列界面样本 **missing=0**。不是整个 Unicode 字库全覆盖结论。PID 33284 native main 返回 0，诊断应用已卸载；[输出](evidence/leader/2026-11-02/d7-device/d7-fonts-device.txt)、`fonts-probe-session.hilog.txt`。

复现探针：将证据目录中的 `font-probe-wrapper.cpp`、`font-probe-CMakeLists.txt` 分别复制为仓库 `build/d7-font-source/main.cpp` 和 `CMakeLists.txt`，再以 Qt Harmony `qt-cmake` 构建/官方签名；包装只将现有探针 stdout 重定向到私有 files/d7-fonts.txt，不改探针检测逻辑。包装的相对 include 依赖这个目录位置。

## 无响应现场与后续交接

中间包 PID 28428 在“LDR 重开→MPD 打开→继续自动输入”的探索序列后不再响应菜单和按键，日志有 `QtWatchdog: AppMainThread is not alive`。已保留 [现场截图](evidence/leader/2026-11-02/d7-device/mpd-focus-check.jpeg) 和 `mpd-hang-session.hilog.txt`，用 `aa force-stop` 结束该测试进程。**该轮没有正常退出证据，不能计通过。**

重启后的 PID 30304 能完成 MPD 系统打开、保存、再次打开、菜单、移动/撤销/重做及窗口操作，最终返回 0。但尚未取得原无响应现场的有效线程栈，不能断言其根因是 MPD、Qt 文件授权、剪贴板或自动化输入。建议组长+A/B 在 D8 用手工操作复现上述序列并采集线程栈；本轮未为此发布远端 issue。

| 负责人 | 仍需完成的交接项 |
|---|---|
| 组长+A | 真实右拖/Shift/Alt 组合、高分辨率或普通滚轮、DPR 和棱线多点拾取；当前没有对应物理鼠标结果 |
| 组长+B | MPD 编辑后保存→退出→重开；实际拒绝授权/无写权限、受控断网；异常存储时恢复副本体验；#15 保持待完整验收 |
| 组长+A/B | 无响应探索序列的手工复现和线程栈，确认影响范围 |
| C | 文件错误正文及标准按钮仍有英文；Part ID 文案；Brick→All Parts 同进程回归和应用材料/隐私说明另行核对 |

## 自动回归和恢复

宿主新增外部保存测试 **9 passed / 0 failed / 0 skipped**，覆盖 Unicode 新建、短内容覆盖尾部裁剪、部分写失败恢复、失败新建清理、目录目标、读回不一致及恢复失败时保留副本。模型回归 **65 passed / 0 failed / 0 skipped**；网络范围单测 **8/8**；TS XML 和 `git diff --check` 通过。原始测试输出在证据目录。Harmony 最终包构建、官方签名、安装及保存原故障复现路径均完成。

测试前应用未运行，结束后应用正常关闭。原配置恢复并读回，沙箱固定 LDR 与两份公开输入副本也读回核对：

| 原数据 | SHA-256 / 结果 |
|---|---|
| LeoCAD.conf（5420 字节） | `5575996fecc88dc99bb3513e4d8d5a68da11521b04685dfc3c6c65fa66a13631`，恢复后相同 |
| render-baseline.ldr（267 字节） | `25a650cddc8a6da11ae8edac3f71328294308244ea1280eaa9a78fbd88a1769b`，沙箱/公开输入均未改动 |
| curated-device.mpd（250 字节） | `ef964c4aeccc8e325b45f0f30835e7b3c924a1f9e6de234ec9362a5d88261742`，公开输入未改动 |

公开目录“下载/LeoCAD-D7-Input”保留本次输入、异常文件和导出副本供复核；初期准备目录“下载/LeoCAD-D7”也为本批次测试产物。原个人配置不入库。证据文件原始字节校验见 `manifest.sha256.json`。
