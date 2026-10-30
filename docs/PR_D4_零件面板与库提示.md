## Purpose

成员 C 的 D4 阶段交付，外加四个缺陷修复。三个业务提交，主题独立，可分别回退：

（缺陷三与缺陷四都在 `64bd33f7` 内——同一次「面板该说什么」的改动。）

| 提交 | 主题 |
|---|---|
| `9dd8395f` | 文件菜单仍显示「导入」「导出」分组（组长真机反馈） |
| `64bd33f7` | 零件面板计数、空态与库异常提示（D4 主体） |
| `e9726bf2` | 时间线面板的 7 个步骤命令仍可触达（自查发现） |

分支上另有 `dd605c91`（合入 `origin/main` 的 B 文档提交）与两个文档提交
（`d2aa6a0f`、`ca89ef90`），不含业务改动。

**缺陷一：菜单裁剪不彻底（组长反馈）。** `CreateMenus()` 用 `FileMenu->addMenu()` 建
子菜单容器，`ApplyFirstReleaseScope()` 只摘 `QAction`。条目全部不可见后，`QMenu` 本身
仍是父菜单里的可见项，点开是空框。

**缺陷二：时间线面板的步骤命令没被裁掉（自查发现）。** 这是同一个「裁剪不彻底」的
第二个面：菜单里的 `LC_VIEW_TIME_*` 被移除了，但时间线面板的右键菜单挂的是**另一套
命令 ID** —— `lcTimelineWidget::CustomMenuRequested()` 直接 `addAction()` 七个
`LC_TIMELINE_*`。同一批操作、两套 ID，所以从菜单路径看「步骤功能已经没了」，
而面板一直提供。面板经 `tabifyDockWidget()` 与属性面板并列，默认存在。

**缺陷三：库被拒只有 stderr。** `PlaceBundledLibraryArchive()` 拒绝归档时只
`fprintf(stderr)`。D3 §3.3 的理由（启动需可无人值守）对启动流程成立，对**用户可见性**
不成立：真机看不到 stderr，症状是「面板只有 128 件」而非任何错误。桌面版的
`LoadPartsLibrary()` 失败弹框走不到这条路径——列表里还有内置库，加载「成功」。

**缺陷四：面板首屏为空**（D4 自查发现）。`UpdateCategories()` 只**恢复**已有选择，
从不建立。新建面板无选择 → 列表视图从未收到 `SetCategory()` → 首屏空白，
而 D2 §6.2 的草图明确画着「分类: [全部 ▼]」且有内容。

D4 要求的「分类/搜索/颜色接入精简库」经排查**在 D3 已完成**：`gCategories` 与
`gColors` 由同一份 `Load()` 重建，D3 真机已用 `10172`/`30293`（内置 128 件库没有）
证明确实加载的是精简库。本 PR 交付的是缺失的后半部分：异常状态下的提示。

## Affected modules

UI/零件库（成员 C 责任范围）。逐项对照分工文档 §3.1：

| 文件 | 归属 | 改动 |
|---|---|---|
| `common/lc_mainwindow.cpp` | C | 菜单容器摘除；7 个时间线命令入移除表；面板接入库异常 |
| `common/lc_partselectionwidget.{h,cpp}` | C | 计数标签、空态覆盖层、库异常横幅 |
| `common/lc_library.{h,cpp}` | C（库发现/诊断部分） | 记录拒绝原因（静态成员） |
| `qt/qtmain.cpp` | 组长 | **仅加 1 行**记录拒绝原因，插入式，不改入口逻辑 |
| `tests/library/d3/tst_menu_scope.cpp` | C | 新增 `noVisibleEmptySubmenus`；移除清单加 7 项 |
| `tests/library/d4/tst_parts_panel.cpp` | C | 新套件，15 项 |
| `d4-panel-tests.pro` | C | 新测试工程 |
| `scripts/dev/run-d3-tests.sh`、`scripts/Run-D3Tests.ps1` | C | 接入 `panel` 套件 |
| `docs/D3_主程序接入.md`、`docs/D4_零件面板与库提示.md`、`tests/library/d3/README.md` | C | 文档 |

未改 `CMakeLists.txt`、`lc_application.*`、平台层。`qt/qtmain.cpp` 的改动需要组长确认，
见「风险与回退」。

移除命令表由 48 项增至 **55** 项（7 个 `LC_TIMELINE_*`）。

## Build and tests

环境：Linux（WSL2 6.18.33.2），Qt 6.12.0（`.devtools/env.sh`），GCC 11.4.0，offscreen。

- [x] Desktop/host build completed
- [x] Relevant automated tests passed
- [ ] HarmonyOS ARM64 build completed, or marked as pending — **pending**，本阶段无鸿蒙工具链
- [x] No unrelated generated files are included

构建：`source .devtools/env.sh && JOBS=2 scripts/dev/run-d3-tests.sh`

> `JOBS=2` 而非更高：GCC 11 在并发内存压力下会随机报 `could not split insn` 或
> `Terminated signal terminated program cc1plus`，与本改动无关，重试即过。

全部五套 **从干净目录重建**（`rm -rf` 后 `make -j1`）后运行：

| 套件 | 项数 | 结果 |
|---|---:|---|
| archive | 13 | pass |
| priority | 8 | pass |
| menu | 8 | pass |
| modellist | 5 | pass |
| **panel（新增）** | **15** | **pass** |
| **合计** | **49** | **49 passed, 0 failed** |

项数为 QtTest 的 `Totals`，含 `initTestCase`/`cleanupTestCase`；panel 套件是
13 个业务用例 + 初始化/清理共 15 项。

新增 `tests/library/d4/tst_parts_panel.cpp`（工程 `d4-panel-tests.pro`），
在真实 `lcMainWindow` 里操作真实面板：

| 用例 | 断言 |
|---|---|
| `countShowsVisibleParts` | 构造完（All Parts）计数为 `Parts: 1160` |
| `healthyLibraryShowsNoBanner` | 健康安装无横幅 |
| `refusedLibraryShowsBannerAndTooltip` | 横幅出现；正文不含内部措辞；tooltip 保留原文 |
| `refusedLibraryDoesNotBlameTheFilter` | 库异常且列表空时，空态**不**出现 |
| `filterWithNoMatchShowsSearchNotice` | 搜索无结果时提「搜索」，计数归 0，清空后消失 |
| `emptyCategoryShowsCategoryNotice` | Sticker 空分类提「分类」，不提「搜索」 |
| `emptyNoticeCoversViewportNotLayout` | 覆盖层父对象是 viewport 且尺寸一致 |
| `bannerFollowsTheTheme` | 横幅取调色板 ToolTip 配色，不用固定色与样式表 |
| `rejectedBundleWithWorkingExternalLibraryIsHealthy` | 包内归档被拒 + 外部库正常：诊断保留，横幅隐藏 |
| `degradedVerdictTracksTheLoadedCount` | 降级谓词边界：1159/128/0 降级，1160/4640 不降级 |
| `optionToggleRefreshesCountAndEmptyState` | 选项开关改变可见集合后，计数与空态同步 |
| `startupEndsOnTheBrickCategory` | 复现真实启动序列，最终分类为 Brick 且非空 |
| `degradedWithoutDiagnosticStillExplainsAnEmptyList` | 无诊断的降级仍显示横幅；空列表至少有一个说明 |

**回归验证** —— 每条断言都实际改坏并确认失败：

| 改坏的行为 | 失败项数 | 报错 |
|---|---:|---|
| 撤回空子菜单容器修复 | 1 | `&Import, &Export, &Render, Ste&p` 四个空框 |
| 从移除表删掉 7 个时间线命令 | 1 | `Set Current Step is still visible in a menu` |
| 去掉构造末尾的 All Parts 选择 | 2 | 计数停在 `Parts: 0` |
| `SetLibraryProblem()` 提前返回 | 5 | 横幅与空态均不出现 |
| 空态忽略 `mLibraryProblem` | 1 | 库损坏时提示「没有匹配的零件」 |
| 横幅跳过 `setPalette` | 1 | 背景不是主题的提示色 |
| 去掉 `VisibleRowsChanged()` 信号 | 5 | `Parts: 0 vs 4 visible`，空态不出现 |
| 恢复「只看包内归档是否被拒」的判据 | 1 | 正常外部库仍显示缺件横幅 |
| 启动用例的期望改成 All Parts | 1 | 与实际启动序列不符 |
| 横幅重新要求非空诊断（两条旧判据） | 1 | 无诊断降级时两条提示都不出现 |

**三处假通过已修正，值得记录：**

1. 空态那条回归**第一次改坏时没有失败**：库正常时列表非空，`VisibleCount == 0`
   不成立，代码根本没执行到。用例补上「先搜索到空、再设库异常」后才生效。
2. 菜单的子菜单判空最初写成 `actions().isEmpty()`，漏掉 Step 菜单——它两组之间留着
   一个分隔符，对用户是空的但 `actions()` 非空。
3. 时间线遗漏之所以长期未发现：两套命令做同一件事、却是两套 ID。补充一个测量细节——
   `LC_VIEW_TIME_DELETE` 与 `LC_TIMELINE_DELETE` **共用标签 "Remove Step"**，而
   `IsCommandVisible()` 按标签查找 QAction，因此这两个命令在测试里**无法区分**：
   检查前者是否已移除时，若后者仍可见，会得到「仍然可见」的误失败；反之亦然。
   按标签抽查对这类重名命令不可靠，本次回归因此以 `Set Current Step`（标签唯一）
   的失败为准。

4. 本 PR 首轮评审又发现三处，都是「测试只覆盖了构造，没覆盖真实运行」：

   - **计数只在输入框变化时刷新**：齿轮菜单直接调模型 `SetFilter()`，绕过刷新点。
     现改为模型在 `SetFilter()` 末尾发 `VisibleRowsChanged()`，覆盖全部 14 处调用点。
   - **首屏声明错误**：`Initialize()` 在 `CreateWidgets()` 之后调用 `SetDefaultPart()`，
     分类被切到 Brick。原测试只到 `CreateWidgets()`，断言了一个用户看不到的状态。
     现已复现真实序列，并修正声明（首屏是 Brick、约 156 件，不是 1160）。
   - **横幅判据错误**：`LoadPartsLibrary()` 会先用 `LEOCAD_LIB`／用户偏好并直接返回，
     包内归档被拒并不等于实际降级。现按最终装载件数判定。

四处与 D3 的 priority 套件同类：**断言了症状，没断言用户看到的状态**。

另：`python3 scripts/library/check_qrc_resources.py` → 156/156 存在，0 缺失。

## Physical-device verification

Commit/artifact: **待组长构建**。业务提交 `e9726bf2`（分支 `feature/c-d4-panel`，最新 `e0946057`），
基线 `main@4302927f`（已合入 A 的 D4 渲染修复与 B 的文档提交）。HAP 由组长用
`scripts/build-harmony-app.ps1` 生成，本 PR 不提供构建产物。

Steps and expected result（按分工文档 §6.1 的批次提交要求，共 8 项，可分批）：

| # | 步骤 | 预期结果 | 归属 |
|---:|---|---|---|
| 1 | 全新启动，观察零件面板 | 首屏**不再是空白**；按上游默认停在 Brick 分类，计数约 `Parts: 156`（不是 1160） | 本 PR |
| 2 | 打开文件菜单 | 无「导入」「导出」分组；菜单为 新建/打开/保存/另存为/最近/退出 | 本 PR（D3 遗留） |
| 3 | 右键时间线面板（属性面板标签旁） | 上下文菜单中 **不出现** Set Current Step / Insert Step / Remove Step / Move Selection 各组 | 本 PR（新增） |
| 4 | 搜索框输入一个不存在的词（如 `zzz`） | 列表区出现覆盖层提示；计数为 `Parts: 0`；清空后提示消失 | 本 PR |
| 5 | 点击 Sticker 分类 | 覆盖层提示「此分类没有零件」，文案**不含**「搜索」字样 | 本 PR |
| 6 | 搜索 `3001`，再打开齿轮菜单关闭 Part ID 匹配 | 列表变空时计数同步变为 `Parts: 0` 并出现空态；**标签不停留在旧数字** | 本 PR（评审新增） |
| 7 | **单独批次**：人为损坏包内 `library-lite.zip` 后启动 | 面板顶部出现横幅，应用仍可用（回退到内置库） | 本 PR |
| 8 | **单独批次**：`LEOCAD_LIB` 指向正常精简库，同时包内归档损坏 | 应用正常使用外部库，**不出现**缺件横幅 | 本 PR（评审新增） |

第 7、8 项需改包或改环境，建议单独一批，勿与常规回归混跑。步骤 1–6 可并入常规批次。

> **观感提示**：本 PR 新增文案目前**只有英文**。`leocad_zh_CN.ts` 缺这些串
> （含既有的 `Filter Parts`、`Change Selection Color`、`Save Project`），属已知问题
> F-02，统一在 D7 语言阶段处理。因此第 4–8 项在真机上会看到英文提示，**不是缺陷**。

> **步骤 3 的注意事项**：时间线面板**仍然可见**（本 PR 只移除了它的 7 个步骤命令，
> 没有隐藏面板）。面板本身用于查看步骤结构，隐藏它属于产品决策。因此步骤 3 验的是
> 「右键菜单里没有步骤编辑项」，不是「面板不见了」。

Required logs/screenshots:

- 步骤 1：零件面板全景截图，需能看清底部计数
- 步骤 2：文件菜单展开截图（对应 D3 交接项 4）
- 步骤 3：时间线面板右键菜单截图（这是本轮新增的 C 项遗留关闭证据）
- 步骤 4–5：覆盖层截图，字体不截断
- 步骤 6：横幅截图 + `hilog` 中含 `Bundled parts library rejected:` 的行
- 通用：`hilog` 中 `libleocad.so ... returned 0`，确认正常退出

## Risk and fallback

**风险**

| 项 | 风险 | 缓解 |
|---|---|---|
| `qt/qtmain.cpp` | 属组长责任文件，本 PR 加 1 行 | 插入式单行调用，不改入口逻辑；组长可单独审这一行或要求改在平台层设置 |
| 时间线命令残留 | `UpdateSelectionUI()` 仍对 3 个 `MOVE_SELECTION` 动作调 `setEnabled()` | `setEnabled()` 不改 visibility，不会让它们回到菜单；该段调用已无意义，建议另开改动删除 |
| `createPopupMenu()` | 工具栏右键菜单每次弹出时重建，`ApplyFirstReleaseScope()` 影响不到 | 它列的是工具栏开关而非步骤命令，不是同一风险；本 PR 不处理 |
| 静态成员 | `mBundledLibraryProblem` 是进程级状态 | 只在启动时写一次，无并发写；归档校验本身无副作用（测试 `validationDoesNotTouchGlobalColours` 守着） |
| 横幅配色 | 鸿蒙平台调色板可能与桌面 Fusion 不同 | 用调色板角色而非固定色，跟随主题；若真机显示不佳，只改 `SetLibraryProblem()` 里一处配色 |
| 覆盖层定位 | 依赖 viewport 事件过滤器 | 已断言尺寸一致；若鸿蒙下异常，可退回为布局项（会挤小列表，但可用） |
| 计数标签 | 鸿蒙字体行高可能不同，标签可能被裁切 | 真机步骤 1 专门检查；若裁切，把 `mCountLabel` 从布局移入状态栏 |

**降级开关**：本 PR 没有新增编译期开关。如需整体关闭面板提示：

1. 单点关闭库横幅：在 `lcMainWindow::CreateWidgets()` 删掉 `SetLibraryProblem(...)`
   那一行，横幅永不出现，其余行为不变。
2. 单点关闭空态：把 `RefreshStatus()` 里的 `ShowEmpty` 恒设为 `false`。
3. 回退时间线裁剪：从 `RemovedCommands[]` 删掉 7 个 `LC_TIMELINE_*`，其余不受影响。
4. 整体回退：三个业务提交主题独立，可只回退其中一个。

**已知未做**（详见 `docs/D4_零件面板与库提示.md` §9、`docs/D3_主程序接入.md` §4.5）：

- 隐藏时间线面板本身：属产品决策，需另行确认
- **首屏默认分类仍是 Brick**（上游行为，本 PR 保留）。若产品希望首屏看全库，
  需在 `Initialize()` 里改 `SetDefaultPart()` 的语义，属独立改动
- `UpdateSelectionUI()` 中对已移除动作的 `setEnabled()` 调用未清理
- 分类名/颜色名中文显示：`lcGetCategoryDisplayName()` / `lcGetColorDisplayName()`
  只在未合入的 `backlog/c-ui-library` 分支，涉及 `lc_colors.h` 固定 `char[64]` 缓冲
  与导出文件名一致性，需单独评审
- `leocad_zh_CN.ts` 缺本 PR 新增文案（F-02，D7 处理）

**顺带确认的非缺陷**：File/Edit/View/Piece 仍存在相邻 `---`。Qt 在布局时把相邻、
开头、结尾的分隔符折叠成 0 高度（已用最小程序实测：两个连续 separator 中第二个
`actionGeometry()` 为空、高度 0），屏幕上不出现横线。**没有**为它加断言——那会是一条
永远无法失败的测试。

**上一版 PR 文档的错误已修正**：`docs/D3_主程序接入.md` 曾写「移除 40 项」，上一轮
我在 PR 里改成 47 项，两者都错。真实数字是：本 PR 之前 48 项，本 PR 之后 55 项。
47 的来源是按「以逗号结尾的行」计数，漏掉了数组最后一项。
