# 成员 B 开发入口

准备日期：2026-10-26；首批开发：2026-10-27；D2 隔离补齐：2026-10-28；D4 核心调用核验：2026-10-30。职责：模型编辑、撤销重做、LDraw 文件读写与数据一致性。

## 当前基线

- 本地目录：`F:\codes\LiteX_trans\leocad-harmony-b`
- 分支：`feature/b-model`
- 团队仓库：`https://github.com/xiaotaop/leocad-harmony.git`，remote 名为 `origin`；本地分支跟踪 `origin/feature/b-model`。
- 上游：`https://github.com/leozide/leocad.git`，remote 名为 `upstream`。
- 固定标签：`v26.09`；提交：`5196355d4a5adb35f14c97e7672d7a9bfd3af391`。
- 首批和第二批已通过 PR #2、#5 合入；D2 从 `main` 的 `cf2adc6`（已含 #7、#8、#9）快进同步后继续。团队保留上游根目录布局。
- 桌面对照：Windows x64、Qt 6.8.3 MSVC2022、MSVC 14.41.34120、Windows SDK 10.0.19041.0。
- Qt 位于本目录 `.local/Qt/`，下载工具位于 `.local/python/`；没有修改全局 PATH、系统 Python 或全局 Git 配置。
- B 本机仍使用桌面工具链；组长环境已完成固定版本的 HarmonyOS ARM64/HAP 与设备测试，见下方 D4 核心调用和 D3 核验记录。桌面结果和设备结果分别记录。

组长方案文件的来源与 SHA-256 记录于 `member-b-baseline.json`。该文件只是 B 的准备记录，不取代组长负责的正式 `toolchain.lock`。

10/28 D2 测试：49 个业务用例通过（含初始化/清理共 51 passed），0 failed、0 skipped，进程退出码为 0。保留 10/27 第二批的 37 个业务用例，新增 12 项格式隔离和离线错误回调用例；不代表完整 GUI 或鸿蒙验收。结果见 `member-b-validation.json`、`member-b-test-results.txt`，包含固定源码与产物哈希。

## D4 核心编辑调用（2026-10-30）

原计划 B 的 D4 要求是“实现/验证插入、选择、移动、旋转、删除的核心调用”。本次从已合入 #18/#19/#20/#21 的 `main@4548f110d83390043b201375cb17ac855fcbd878` 核验；复用已有真实调用和测试，不新增业务补丁或重复测试。

| D4 项目 | 已有核心调用与桌面断言 |
|---|---|
| 插入 | `insertTwentyPiecesUndoRedoAndReopen` 调用 `AddPiece` 连续插入 20 件，逐次核对数量，并验证撤销、重做及保存重开后的 20 件 |
| 选择 | `editingSequenceUndoRedoAndSave` 调用 `SetSelectionAndFocusAction` 选择首件，随后通过该件移动、旋转、改色及选中删除的结果验证选择生效；不替代鼠标拾取精度验收 |
| 移动 | `MoveSelectedObjects` 后断言首件位置为 `(20,0,0)`，撤销恢复原位，保存重开保留新位置 |
| 旋转 | `RotateSelectedObjects` 应用固定 90 度矩阵，断言旋转矩阵前两列，并验证撤销恢复及重开保留 |
| 删除 | `DeleteSelectedObjects` 后由两件变一件，撤销恢复两件，重做回到一件 |

上述用例位于 [模型测试](../tests/model/tst_model_io.cpp)，使用隐藏的真实 Qt 主窗口和模型命令；桌面合成零件不证明正式几何或鼠标操作。

**本次集成回归**：在上述 `4548f110` 上执行 `scripts/Build-MemberB.ps1 -Target Tests -RunTests`，模型测试构建通过，49 个业务用例通过（含初始化/清理共 **51 passed、0 failed、0 skipped**），退出码 0，见 [D4 测试结果](member-b-d4-test-results.txt)。首次受限构建遇到 MSVC `C1902` 程序数据库管理器错误、未进入测试；以正常本机权限执行相同命令后通过，没有修改源码、构建配置或断言。本次未另构建完整桌面应用，也未重跑 ARM64 测试包。

**已有真机补充证据**：组长在 `fbcdc5e11be8a9dc46471b8ac0990cb603982740` 上使用 HAD-W32 / ARM64 / API23 完成了关闭 WLAN 后的正式零件选择、插入、移动、旋转、删除、撤销、沙箱保存和退出重开，见 [资源与基础编辑真机报告](D3_RESOURCES_DEVICE_ACCEPTANCE.md)。本次重算 [收据](evidence/leader/2026-10-29/d3-resources/receipt.json) 中 23 个文件的字节数及 SHA-256，全部匹配；核对两张插入/重开截图、五份逐步保存的 MPD 和 PID 9110、10033 的 main 返回 0 日志。

- `3005.dat` 插入后为 `(45,-24,0)`、单位矩阵；移动后为 `(45,-24,-10)`；旋转后矩阵为 `0.866025 0.5 0 / -0.5 0.866025 0 / 0 0 1`。这里是文件中的 LDraw 坐标，不与桌面用例的内部坐标混用。
- 删除后该件消失，原有零件及子模型结构保留；撤销后的文件与旋转后文件逐字节一致。
- 截图的目视复核：`offline-insert2.jpeg` 中新增砖块有选中着色和操纵器，`offline-reopen.jpeg` 中该砖块保留旋转姿态，原有红/黄零件仍可见。截图只能补充外观证据；坐标、旋转矩阵和子模型结构以 MPD 为准，不能从图片证明所有操作或关闭 Issue #15。
- 本次仅复核仓库证据，未操作设备或取得 HAP 重算包哈希。`fbcdc5e` 至本次基线的模型、历史、零件、项目文件及模型测试源码无差异，但其间渲染和资源代码有更新，不能将旧包记录扩展为最新包完整验收。

系统选择器、授权、外部导出及取消/失败路径仍按 [Issue #15](https://github.com/xiaotaop/leocad-harmony/issues/15) 的 D7–D8 验收。已有沙箱离线编辑/重开证据只完成其中一部分；D4 核心调用交付不关闭该 Issue，也不替代 A/C/组长的 D4 出口。

B 的 D4 五项核心调用均有验证，可按上述版本和范围交付。

## D3 对照与核验（2026-10-29，保留阶段记录）

原计划 B 的 D3 两项要求已有证据：核心模型通过 ARM64 编译；新建空模型测试通过。PR #10、#11、#17 已合入，B 分支已快进同步到 `37e56526f8842ae4ae9eec31fa33e2a78c1f9c0c`。

- **ARM64**：复核组长提交 `f0dfce8651d7d42c8c37629294d8727107c8ce1e` 的设备报告，Qt 6.12.0 / OHOS Clang 15.0.4 / arm64，50 passed、0 failed、0 skipped（48 个业务用例及初始化/清理）。同 PID 43199 的测试出口和原生 main 返回值均为 0；[收据](evidence/leader/2026-10-28/d3-device/receipt.json)中 12 个证据文件的 SHA-256 全部匹配。本次复核已有记录，没有重新构建或运行设备包，也未取得组长本机 HAP 重算包哈希。
- **新建空模型**：`emptyModelRoundTrip` 使用真实 `Project(true)`，保存后重开并断言一个模型、零个零件；设备报告与本次桌面回归均通过。复用已有覆盖，不增加重复测试。
- **合入后的桌面回归**：在上述 `37e5652` 上执行 `scripts/Build-MemberB.ps1 -Target Tests -RunTests`，编译通过，49 个业务用例通过，含初始化/清理共 51 passed、0 failed、0 skipped，退出码 0。记录见 [D3 桌面测试结果](member-b-d3-test-results.txt)。首次受限执行中临时文件创建失败（12 passed、39 failed）；在正常本机权限下使用相同源码和命令复跑通过，未修改断言。
- **版本边界**：设备证据对应 `f0dfce86`；C 的 #17 后续修改了零件库、菜单及 `Project::ShowModelListDialog`。本次最新集成版只补做桌面模型回归，不将旧设备记录当作 #17 的完整真机验收。

D3 个人任务可按上述范围交付。系统选择器、导入副本/外部导出、取消/权限、物理断网及真实 UI 的保存退出重开，继续由组长+B 在 [Issue #15](https://github.com/xiaotaop/leocad-harmony/issues/15) 的 D7–D8（11/2–11/3）集成阶段验收，不在本次关闭。

## D2 对照与隔离边界（保留原阶段记录）

2026-10-28 组长已在 HAD-W32/API23 上执行独立 ARM64 测试 HAP：50 passed、
0 failed、0 skipped（Windows 专属文件锁用例不编译）；主程序 3DS 入口亦确认拒绝。
详见 [D3 集成验收及原始设备证据](D3_DEVICE_ACCEPTANCE.md)。此为后续组长补测，
不改写下文 B 原始桌面验证记录；系统文件选择及整套人工建模/断网验收仍待后续联调。

初始分工 D2 要求：“将模型/历史测试扩展为可独立构建；隔离非核心格式和网络路径”。前一项已有独立 Qt Test 可执行目标；本次补齐后一项。

| 项目 | D2 交付 |
|---|---|
| 独立构建 | 继续复用上游源清单和真实模型/历史调用，不新增核心库；仍依赖隐藏 Qt Widgets 和桌面 OpenGL |
| 网络路径 | 测试启用已有 `LC_DISABLE_NETWORK`、禁用更新/外部渲染/Blender，移除 Qt Network 和 WinInet 链接；检查测试 EXE 的直接依赖，离线下载入口异步返回错误 |
| 非核心格式 | `LC_DISABLE_NON_CORE_FORMATS` 在 Project 入口禁用旧二进制读取、LDD/LXF 导入，以及 3DS/COLLADA/CSV/HTML/POV-Ray/OBJ/BrickLink 导出；在文件写入、目录创建或替换文档之前拒绝操作 |
| 开关接入 | B 测试默认启用；根 CMake 仅在 `OHOS` 目标启用。普通桌面构建不启用，保持原有格式行为 |
| 保留范围 | LDraw `.ldr/.mpd/.dat` 解析及原有文本格式探测、编辑、历史、安全保存、子模型往返保留；LDraw 步骤/子模型序列化和图像诊断出口不属于本次格式禁用 |

隔离测试覆盖已有输出文件内容不变、文档身份/活动模型/未保存状态不变、HTML 输出目录不创建和离线请求完成回调。`empty-model.lxf` 是自行生成的最小 ZIP，内含 `IMAGE100.LXFML` 的空模型，用于验证显式 LDD 入口拒绝。

修复前已观察到 3DS/COLLADA/CSV 导出仍返回成功；完整探测随后在 POV-Ray 对 GUI 的既有依赖处中止，不能将该次运行当作完整测试结果。修复后 51 项全部通过。普通桌面应用干净构建、版本启动（26.09）和固定样例 OBJ 导出通过，导出含 488 个顶点、440 个面。

此项是 B 的构建与模型 API 隔离，不替代 C 的菜单裁剪，也不声明鸿蒙生产包/权限/系统选择器已验收；下层旧解析器源码仍保留。D3 ARM64 编译与后续真机闭环继续由集成批次验证。

## 直接使用

在项目根目录打开 PowerShell：

```powershell
# 构建并运行真实模型/文件测试
.\scripts\Build-MemberB.ps1 -Target Tests -RunTests

# 构建当前分支桌面应用
.\scripts\Build-MemberB.ps1 -Target App
```

从 D1/第二批升级或修改编译开关时，首次运行追加 `-Clean`，避免 qmake 复用旧开关下的 moc 文件；清理只针对所选目标的构建产物。CI 使用干净检出，无需每次清理。

脚本自动定位已安装的 Visual Studio C++ 工具链，仅在当前调用中配置环境，并在结束时恢复。
使用其他 Qt MSVC 套件可传 `-QtRoot '绝对路径'`；不同 Qt/编译器应使用新的构建目录，不能混用旧对象文件。

- 测试日志：`.local/build-tests/test-results.txt`
- 网络依赖检查：`.local/build-tests/dependencies.txt`（与构建/测试日志一同由 CI 归档）
- 构建日志：`.local/build-tests/build.log`、`.local/build-app/build.log`
- 测试程序：`.local/build-tests/build/release/leocad-model-tests.exe`
- 桌面应用：`.local/build-app/build/release/LeoCAD.exe`

这些是开发产物，依赖本地 Qt DLL；不能将单个 EXE 当作可分发安装包。手动启动桌面应用需将本地 Qt `bin` 加入当前进程 PATH，并另备零件库。本次不下载完整库、不承担 C 的正式资源筛选。

## 已建立的独立验证入口

上游固定提交**没有 `tests/` 目录或 Qt Test 工程**，其现有 CI 主要负责构建。新增 `member-b-tests.pro` 复用 `leocad.pro` 的源文件清单，替换程序入口、添加 Qt Test 并启用上述隔离开关；未抽取新的核心库。D2 对正式根 CMake 的改动仅为 OHOS 目标增加格式开关，业务改动仍限定在 `common/project.cpp`。

`tests/model/tst_model_io.cpp` 调用真实 `Project::Load/Save`。最初五项文件测试保留，并增加保存失败、编辑与历史、格式错误和兼容用例：

1. 空模型保存、重开，保持一个模型、零个零件。
2. 两个零件的 `.ldr` 往返，校验零件 ID、颜色、位置、旋转及中文带空格的输出路径。
3. 包含子模型的 `.mpd` 往返，校验模型名称、引用关系、数量和颜色。
4. 打开不存在的文件失败，原模型名称和数量保持不变。
5. 将目录作为保存目标时返回非空错误。

样例是自行编写的序列化数据，`b_fixture_brick.dat` 故意采用上游占位机制，不依赖正式 LDraw 零件库；这不验证零件几何、纹理、库完整性或实际渲染。

测试不展示窗口，但上游 `lcPiecesLibrary` 的析构依赖全局图形上下文，因此入口仍初始化和释放桌面 OpenGL。它不是纯无图形环境测试；不应直接承诺可在任意无 GPU CI 上运行。配置使用独立测试名称和临时 INI 文件，不读取正常 LeoCAD 用户配置。

`.github/workflows/model-ci.yml` 在面向 `main` 的 PR、`main` 推送和手动运行时执行 Windows 桌面对照：固定 Qt 6.8.3，使用 Qt 提供的软件 OpenGL，复用上述脚本运行全部模型测试、构建完整应用并检查版本启动。构建和测试日志作为 Actions artifact 保存 14 天。CI 不覆盖鸿蒙 ARM64、真机或实际渲染效果。

## B 的源码入口和现有边界

| 任务 | 实际入口 | 注意事项 |
|---|---|---|
| 项目读写 | `common/project.cpp`：`Project::Load`、`Save(QString)`、`Save(QTextStream&)` | 无弹窗读取和文件保存返回 `lcResult<void>`；旧的读取 bool 接口兼容保留 |
| LDraw/MPD | `common/lc_model.cpp`：`SplitMPD`、`LoadLDraw`、`SaveLDraw` | 不要另写一个解析器替代真实上游测试 |
| 零件状态 | `common/piece.*` | 位置、旋转、颜色、显示步骤和序列化 |
| 编辑 | `common/lc_model.cpp`：`AddPiece`、`MoveSelectedObjects`、`RotateSelectedObjects`、`DeleteSelectedObjects`、`SetSelectedPiecesColorIndex` | 多处直接访问 `gMainWindow`；不能当作已有纯核心 API |
| 历史 | `common/lc_modelhistory.*`，`lcModel::UndoAction/RedoAction` | 原功能已存在，先验证再适配；撤销重做仍有主窗口回调 |
| 底层文件 | `common/lc_file.*` | 不接管组长的系统文件选择和权限实现 |

## 首批交付与后续边界

安全保存使用 `QSaveFile`，禁用直接覆盖回退；确认刷写及文件替换成功后才更新保存点。打开文件增加无弹窗的 `lcResult<void> Load(const QString&)`，旧的 `Load(path, ShowErrors)` 保持兼容。平台层可以读取错误文本并自行呈现。

已验证：实际插入 20 件、选择/移动/旋转/改色/删除、撤销/重做、保存点恢复、新编辑清除重做；这些通过隐藏的上游主窗口调用真实命令，没有修改原有编辑/历史实现。

格式保护覆盖：空文件、Type 1–5 基本几何字段、非有限数值、过长/缺失引用名、重复/空 MPD 子模型名。未知行和 META 保持兼容，BOM 和直接颜色可读取；`0 !DATA` 内嵌二进制明确返回不支持。参考 [LDraw 基本格式](https://www.ldraw.org/article/218.html)及 [MPD 扩展](https://www.ldraw.org/article/47.html)。

第二批增加 MPD 内部引用环检查，覆盖自引用、相互引用及未被根模型使用的循环组件；先拒绝输入，避免上游静默丢弃引用。合法的多父节点共享子模型仍可往返。SYNTH CONTROL_POINT 的 13 个数值和 PIECE PIVOT 的 12 个数值必须完整且有限；NOFILE 后的无效 META 文本仍按格式忽略。以上连同路径场景在修复前共复现 9 个失败，修复后全部通过。

本地路径边界：成功打开和保存后，项目文件名保存为绝对路径。测试在中文带空格的文档目录打开/保存，再切换工作目录到缓存目录，确认后续保存与重开仍访问原文档，不在缓存目录误写。平台层仍负责提供有权限的本地路径；这些桌面目录测试不等同鸿蒙授权或真实沙箱验收。

后续仍需按首发范围评估其他扩展 META（相机/灯光/关键帧/分组等）异常输入、外部文件引用循环/规模限制，并完成系统导入导出及完整 UI 验收。ARM64 模型测试已在 D3 取得固定版本证据，正式零件的部分编辑/沙箱重开也已有上述 D4 复核记录，不再笼统列为未完成；当前仍不是完整格式验证器或发布放行。

组长接入时建议先在新建 `Project` 上调用无弹窗 Load，成功后通过现有 `SetProject` 切换 UI；取消选择时不要调用 Load。保存返回成功仅表示本地目标提交成功，外部导出仍由平台层确认。

后续真机验收：固定版本已有模型 API 测试，以及正式零件的选择/编辑/撤销和沙箱保存退出重开证据，见 D3/D4 记录，不再将这些整体列为等待首个 HAP。尚需在实际集成包中验证系统选择器加载 LDR/MPD、取消/权限、外部导出，以及通过 UI 导入截断文件/循环 MPD/坏 SYNTH/PIVOT 时原文档保留、不可写目标失败提示及未保存状态、中文路径和同名覆盖；撤销后的重做 UI 路径也需明确记录。按 Issue #15 汇总每个版本实际通过与待验项目，记录源提交、包哈希、设备/API 和失败日志。第二批可单独撤销 `47b723f`；首批按主题撤销 PR #2 的两个 `fix(model)` 提交。回退后不能继续声明相应数据保护验收通过。

## 与其他成员的最小协作约定（提议，待团队确认）

- B 先基于固定源码和自制样例推进，不等待 A 的鸿蒙渲染或 C 的零件库。
- 组长负责系统选择器、授权、复制到沙箱、外部导出提交与取消/失败状态；B 接收可访问的本地路径，负责解析与安全保存。
- A/C 调用现有编辑入口；涉及 `project.*`、模型状态或命令签名的变更先与 B 对齐。
- 不把“沙箱保存成功”当作“外部导出成功”；用户提示必须对应实际完成的阶段。
- B 本地测试交付与最终 HAP 真机验收分开记录。组长需要回传提交号、包哈希、复现步骤和设备日志。
- 文档中 11/3 完整闭环冻结与 B 11/5 文件闭环 DDL 不一致，需组长统一；B 优先按 11/3 集成闭环安排。

## 团队仓库协作

已经阅读根目录 `CONTRIBUTING.md`。B 继续在 `feature/b-model` 工作，正式提交按单一主题组织。PR 使用 `.github/pull_request_template.md`，写明构建、测试、待真机项及回退方法；至少一名非作者批准，由组长合入 `main`。

后续同步前先保存本地改动，再获取团队更新并检查差异。不要覆盖团队根构建配置，也不要将 `.local/`、Qt SDK、原始构建日志、签名材料或用户模型提交到仓库。`docs/member-b-test-results.txt` 是精简、无设备敏感信息的测试证据。

## 在另一台 Windows 机器恢复桌面工具

需要已有 Python 3.11+、Visual Studio 2022 C++ Build Tools、Windows SDK 和 7-Zip。以下仅安装到本目录：

```powershell
python -m venv .local/python
.\.local\python\Scripts\python.exe -m pip install aqtinstall==3.3.0
# 下面的环境变量仅用于此 PowerShell 进程；也可在专用终端中执行。
$env:APPDATA = (Resolve-Path .local).Path
New-Item -ItemType Directory -Force .local/tmp | Out-Null
$env:TEMP = (Resolve-Path .local/tmp).Path
$env:TMP = $env:TEMP
.\.local\python\Scripts\python.exe -m aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 --archives qtbase qttools qtsvg --outputdir .local/Qt --external 'C:\Program Files\7-Zip\7z.exe'
```

若执行环境限制编译器或解包进程，应在有相应本地执行权限的终端重试，不关闭证书验证或修改全局网络配置。
