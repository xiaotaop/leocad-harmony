# 成员 B 开发入口

准备日期：2026-10-26。职责：模型编辑、撤销重做、LDraw 文件读写与数据一致性。

## 当前基线

- 本地目录：`F:\codes\LiteX_trans\leocad-harmony-b`
- 分支：`feature/b-model`
- 团队仓库：`https://github.com/xiaotaop/leocad-harmony.git`，remote 名为 `origin`；本地分支跟踪 `origin/feature/b-model`。
- 上游：`https://github.com/leozide/leocad.git`，remote 名为 `upstream`。
- 固定标签：`v26.09`；提交：`5196355d4a5adb35f14c97e7672d7a9bfd3af391`。
- 已同步团队初始化提交 `959fb4eee3e020e2579fdf7b4196c9856160f759`。团队保留上游根目录布局，无需搬移模型源码或本地工具。B 的准备增量仍在本地，尚未提交、推送或创建 PR。
- 桌面对照：Windows x64、Qt 6.8.3 MSVC2022、MSVC 14.41.34120、Windows SDK 10.0.19041.0。
- Qt 位于本目录 `.local/Qt/`，下载工具位于 `.local/python/`；没有修改全局 PATH、系统 Python 或全局 Git 配置。
- HarmonyOS 的 Qt 6.12、ARM64 工具链、HAP 和真机验收尚未配置，桌面基线不能替代这些结果。

组长方案文件的来源与 SHA-256 记录于 `member-b-baseline.json`。该文件只是 B 的准备记录，不取代组长负责的正式 `toolchain.lock`。

本次实测：上游桌面应用构建成功；`--version` 输出 `26.09`、退出码为 0；五项业务测试通过（Qt Test 连同初始化/清理共 7 passed），测试进程退出码为 0。完整 GUI、编辑/撤销、异常文件拒绝与鸿蒙适配尚未验收。可追溯结果见 `member-b-validation.json`、`member-b-test-results.txt`，包含测试源码和产物哈希。

团队初始化仅增加文档、目录说明和忽略规则，未改变上游业务源码或构建入口；本地测试源文件哈希也保持一致，因此沿用上述验证结果，本次仓库同步未重复构建。

## 直接使用

在项目根目录打开 PowerShell：

```powershell
# 构建并运行真实模型/文件测试
.\scripts\Build-MemberB.ps1 -Target Tests -RunTests

# 构建未改业务源码的上游桌面应用
.\scripts\Build-MemberB.ps1 -Target App
```

脚本自动定位已安装的 Visual Studio C++ 工具链，仅在当前调用中配置环境，并在结束时恢复。
使用其他 Qt MSVC 套件可传 `-QtRoot '绝对路径'`；不同 Qt/编译器应使用新的构建目录，不能混用旧对象文件。

- 测试日志：`.local/build-tests/test-results.txt`
- 构建日志：`.local/build-tests/build.log`、`.local/build-app/build.log`
- 测试程序：`.local/build-tests/build/release/leocad-model-tests.exe`
- 桌面应用：`.local/build-app/build/release/LeoCAD.exe`

这些是开发产物，依赖本地 Qt DLL；不能将单个 EXE 当作可分发安装包。手动启动桌面应用需将本地 Qt `bin` 加入当前进程 PATH，并另备零件库。本次不下载完整库、不承担 C 的正式资源筛选。

## 已建立的独立验证入口

上游固定提交**没有 `tests/` 目录或 Qt Test 工程**，其现有 CI 主要负责构建。本次新增 `member-b-tests.pro`，复用 `leocad.pro` 的源文件清单，只替换程序入口并添加 Qt Test；未抽取新的核心库、未改动业务源码和正式构建文件。

`tests/model/tst_model_io.cpp` 调用真实 `Project::Load/Save`，包含五项业务测试：

1. 空模型保存、重开，保持一个模型、零个零件。
2. 两个零件的 `.ldr` 往返，校验零件 ID、颜色、位置、旋转及中文带空格的输出路径。
3. 包含子模型的 `.mpd` 往返，校验模型名称、引用关系、数量和颜色。
4. 打开不存在的文件失败，原模型名称和数量保持不变。
5. 将目录作为保存目标时返回非空错误。

样例是自行编写的序列化数据，`b_fixture_brick.dat` 故意采用上游占位机制，不依赖正式 LDraw 零件库；这不验证零件几何、纹理、库完整性或实际渲染。

测试不展示窗口，但上游 `lcPiecesLibrary` 的析构依赖全局图形上下文，因此入口仍初始化和释放桌面 OpenGL。它不是纯无图形环境测试；不应直接承诺可在任意无 GPU CI 上运行。配置使用独立测试名称和临时 INI 文件，不读取正常 LeoCAD 用户配置。

## B 的源码入口和现有边界

| 任务 | 实际入口 | 注意事项 |
|---|---|---|
| 项目读写 | `common/project.cpp`：`Project::Load`、`Save(QString)`、`Save(QTextStream&)` | 文件路径入口已经存在；读取返回 bool，保存使用 `lcResult<void>` |
| LDraw/MPD | `common/lc_model.cpp`：`SplitMPD`、`LoadLDraw`、`SaveLDraw` | 不要另写一个解析器替代真实上游测试 |
| 零件状态 | `common/piece.*` | 位置、旋转、颜色、显示步骤和序列化 |
| 编辑 | `common/lc_model.cpp`：`AddPiece`、`MoveSelectedObjects`、`RotateSelectedObjects`、`DeleteSelectedObjects`、`SetSelectedPiecesColorIndex` | 多处直接访问 `gMainWindow`；不能当作已有纯核心 API |
| 历史 | `common/lc_modelhistory.*`，`lcModel::UndoAction/RedoAction` | 原功能已存在，先验证再适配；撤销重做仍有主窗口回调 |
| 底层文件 | `common/lc_file.*` | 不接管组长的系统文件选择和权限实现 |

## 正式开发时优先处理的缺口

以下是固定源码检查发现，尚未修复；现有五项测试不代表这些要求已经满足。

1. **安全保存**：`Project::Save(QString)` 使用 `QFile` 直接打开目标，保存前清空项目文件名；未采用临时文件提交。`Save(QTextStream&)` 无条件返回 true，并在最终落盘确认前调用 `SetSaved()`。应先增加失败保存的回归测试，再修复目标文件保护、写入错误传播与修改标记。
2. **异常输入**：读取过程清空现有模型后才解析；Type 1 行的数值读取没有完整的流状态校验。需要确定截断、非法数值、重复子模型、缺失零件的策略，不能把所有未知注释行判为损坏。
3. **编辑/历史测试**：优先使用上游真实命令和最小桌面窗口装配；若确需拆开界面回调，只做与 A/C 对齐后的最小改动，不重构完整模型架构。
4. **ARM64 编译及鸿蒙路径**：等待组长提供准确 Qt/SDK、工程接入方式及可用构建入口后补做；本次不声称完成。

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
