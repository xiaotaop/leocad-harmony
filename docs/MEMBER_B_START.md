# 成员 B 开发入口

准备日期：2026-10-26；首批开发：2026-10-27；D2 隔离补齐：2026-10-28。职责：模型编辑、撤销重做、LDraw 文件读写与数据一致性。

## 当前基线

- 本地目录：`F:\codes\LiteX_trans\leocad-harmony-b`
- 分支：`feature/b-model`
- 团队仓库：`https://github.com/xiaotaop/leocad-harmony.git`，remote 名为 `origin`；本地分支跟踪 `origin/feature/b-model`。
- 上游：`https://github.com/leozide/leocad.git`，remote 名为 `upstream`。
- 固定标签：`v26.09`；提交：`5196355d4a5adb35f14c97e7672d7a9bfd3af391`。
- 首批和第二批已通过 PR #2、#5 合入；D2 从 `main` 的 `cf2adc6`（已含 #7、#8、#9）快进同步后继续。团队保留上游根目录布局。
- 桌面对照：Windows x64、Qt 6.8.3 MSVC2022、MSVC 14.41.34120、Windows SDK 10.0.19041.0。
- Qt 位于本目录 `.local/Qt/`，下载工具位于 `.local/python/`；没有修改全局 PATH、系统 Python 或全局 Git 配置。
- HarmonyOS 的 Qt 6.12、ARM64 工具链、HAP 和真机验收尚未配置，桌面基线不能替代这些结果。

组长方案文件的来源与 SHA-256 记录于 `member-b-baseline.json`。该文件只是 B 的准备记录，不取代组长负责的正式 `toolchain.lock`。

10/28 D2 测试：49 个业务用例通过（含初始化/清理共 51 passed），0 failed、0 skipped，进程退出码为 0。保留 10/27 第二批的 37 个业务用例，新增 12 项格式隔离和离线错误回调用例；不代表完整 GUI 或鸿蒙验收。结果见 `member-b-validation.json`、`member-b-test-results.txt`，包含固定源码与产物哈希。

## D2 对照与隔离边界

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

仍待完成：其他扩展 META（相机/灯光/关键帧/分组等）异常输入、外部文件引用循环/规模限制、真实零件库、完整 UI、ARM64 和鸿蒙系统导入导出。当前不是完整格式验证器或发布放行。

组长接入时建议先在新建 `Project` 上调用无弹窗 Load，成功后通过现有 `SetProject` 切换 UI；取消选择时不要调用 Load。保存返回成功仅表示本地目标提交成功，外部导出仍由平台层确认。

真机请求（待可用集成 HAP）：1. 加载固定 LDR/MPD；2. 编辑并撤销/重做；3. 保存、退出、重开；4. 导入截断文件、循环 MPD 和坏 SYNTH/PIVOT，确认原文档仍在；5. 模拟不可写目标，确认失败提示及未保存状态；6. 验证中文文档路径和同名覆盖。需记录源提交、包哈希、设备/API 和失败日志。第二批可单独撤销 `47b723f`；首批按主题撤销 PR #2 的两个 `fix(model)` 提交。回退后不能继续声明相应数据保护验收通过。

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
