# 成员 C D9：界面收口修复与证据补齐

D9 行给成员 C 的任务是「修复布局、字体、焦点、菜单状态；完善测试矩阵」。本文记录本轮做了什么、**没有**做什么，以及每个结论的取证方式。

测试矩阵本身单独成文：[D9_测试矩阵.md](./D9_测试矩阵.md)。

## 一、先说结论

本轮只有**一个**真实的用户可见缺陷被修掉：首发范围裁剪把 Time 工具栏的全部五个命令移除后，留下一条空工具栏仍占着布局一整行（实测 473×39 像素），同时 View ▸ Toolbars 里还留着一个能把它开关的复选项。

另外两条曾被当作"缺陷"的怀疑，经实测**不成立**，本轮没有为它们改任何代码（见第三节）。把不成立的东西写成修复，是比漏修更坏的事——它会让后来的人以为那段代码有问题而再改一遍。

## 二、修掉的缺陷：被裁剪掏空的工具栏仍然占位

### 现象与量化

`ApplyFirstReleaseScope()`（`common/lc_mainwindow.cpp`）按首发范围逐条移除越界命令。它对菜单做了善后——"把子菜单的最后一条删掉，就得把子菜单一起删掉"，这条规则 D3 真机验收时已经补上。但同一条规则**没有用在工具栏上**。

Time 工具栏由五条命令构成（`LC_VIEW_TIME_FIRST/PREVIOUS/NEXT/LAST/ADD_KEYS`），五条全部在移除列表中。移除后实测（探针直接构造成真实的 `lcMainWindow` 并读取控件状态）：

```
Standard | visibleActions=11 | visible=1
Time     | visibleActions=0  | visible=1 | size=473x39
Tools    | visibleActions=19 | visible=0 | (CreateToolBars() 里本就 hide())
```

`visibleActions=0` 而 `visible=1`：工具栏还活着，里面一个按钮都没有，布局照旧给它留一行。在真机上这就是视口上方一条空白带。同时 `LC_VIEW_TOOLBAR_TIME` 仍在 View ▸ Toolbars 里，用户可以把这条空带子关掉再打开。

### 修法

在 `ApplyFirstReleaseScope()` 末尾加一次清扫，规则与菜单那条一致："容器的最后一条被删掉，容器也要删掉"：

- `HideToolBarIfEmptied()`：工具栏没有可见动作（分隔线不算）→ `hide()`，并把对应的开关动作 `setVisible(false)` + `setEnabled(false)`；
- 只处理 `parentWidget() == this` 的工具栏：预览停靠面板自己建了一个 `QToolBar`（`lc_previewwidget.cpp`），主窗口的范围决策不该伸进去；
- `ToolBarToggleCommand()` 按**指针**匹配，不按 objectName —— 预览面板那个工具栏的 objectName 也叫 `Toolbar`；
- 同一段清扫在 `CreateWidgets()` 的 `restoreState()` **之后**再跑一次。

最后一条是本轮踩到的坑，值得写下来：`ApplyFirstReleaseScope()` 由 `CreateMenus()` 调用，而 `CreateMenus()` 在 `CreateWidgets()` 里 `restoreState()` **之前**执行。`restoreState()` 会把保存的布局（含工具栏的可见标志）盖回去，所以只在裁剪时隐藏，对**已有配置文件的老用户**会被下一次启动还原成空带子。第二次清扫就是为此存在的。

### 为什么动作对象只隐藏不删除

`UpdateDockWidgetActions()` 无条件解引用 `mActions[LC_VIEW_TOOLBAR_*]`，`createPopupMenu()` 也直接把它加进菜单。删掉会崩。隐藏的 `QAction` 同时从 View 菜单和标题栏弹出菜单里消失（已实测：菜单不会为不可见动作留下条目），这正是要的效果。

### 验证

新增 3 项用例（`tests/library/d3/tst_menu_scope.cpp`，套件从 8 项增至 11 项）：

| 用例 | 断言 |
| --- | --- |
| `noVisibleEmptyToolbars` | 主窗口没有任何"可见但无可见动作"的工具栏 |
| `emptiedToolbarToggleIsNotOffered` | Time 工具栏已隐藏；其开关动作仍**存在**（不崩的前提）、但不可见且不可用 |
| `toolbarsWithCommandsStayVisible` | 对照组：Standard 工具栏仍有动作、仍可见、其开关仍在 |

三处变异全部被抓到，且报错信息指对了方向：

- 去掉两处清扫 → `noVisibleEmptyToolbars` 与 `emptiedToolbarToggleIsNotOffered` 双红；
- 只隐藏工具栏、不处理开关 → `emptiedToolbarToggleIsNotOffered` 红（`the View menu still offers a toggle for an empty toolbar`）；
- 只去掉裁剪处的清扫（保留 `restoreState()` 后那次）→ 全绿。这一处**故意**保留，因为单靠 `restoreState()` 之后那次已能覆盖无配置文件的首启；裁剪处那次是让"范围决策"这件事本身自洽，删掉不影响行为。

第三点如实写出来：写在 `ApplyFirstReleaseScope()` 里的清扫在本轮测试路径下是**冗余**的。保留它的理由是语义归属（是范围裁剪掏空了工具栏，就该由范围裁剪收拾），但如果组长认为"不留冗余路径"更重要，删掉那 4 行不会让任何用例变红。

## 三、两条被证伪的怀疑（本轮未改代码）

D8 结束时留了几条"疑似菜单状态缺陷"。本轮逐条实测，两条不成立。

### 3.1 双分隔线 / 前导分隔线：Qt 自己会收

`menu->actions()` 里确实能看到 File 菜单有两组相邻的分隔线、Help 菜单开头有一条分隔线。只看动作列表，这像是"裁剪没收拾干净"。

但它不是画出来的。Qt 的 `QMenu::separatorsCollapsible` **默认就是 true**，相邻分隔线在布局时被合并，前导/尾随分隔线被折叠成 0×0。用几何探针在真实菜单上量：

```
leading separators actually drawn:  0
adjacent separators actually drawn: 0
（Help 的首条 sep 几何尺寸 sep(0x0)）
```

用户在屏幕上看到的分隔线数量是对的。**结论：不是缺陷，不改。** 这也解释了为什么 D3/D8 的真机截图里没人报过这个"问题"——它不存在。

### 3.2 被裁剪命令的快捷键会不会被触发：不会

首发范围只把动作 `setVisible(false)` / `setEnabled(false)`，动作对象仍在窗口上（`addAction()` 在 `CreateActions()` 里做过）。如果隐藏动作的快捷键仍能触发，那裁剪就只是视觉上的，用户按 `Ctrl+F` 还能打开"查找"对话框。

实测（`QTest::keyClick` 打到真实窗口）：

```
visible + enabled  → fired: 1   （对照组）
hidden  + enabled  → fired: 0
hidden  + disabled → fired: 0
```

**结论：不成立，不改。** 隐藏动作的快捷键不触发，裁剪在键盘路径上也是真的。

这两条都写进这里而不是悄悄跳过，是因为 D8 文档里把它们列成了待办。待办的正确关闭方式有两种：修掉，或者证明它不成立。后者也是结论。

## 四、菜单分隔线的追加清理：为什么只删尾随的

`ApplyFirstReleaseScope()` 早先只删尾随分隔线（`lc_mainwindow.cpp` 末尾那个循环），不删前导与相邻的。本轮确认这是**正确的克制**：前导和相邻由 Qt 折叠，删它们属于改上游默认行为；尾随那条则不同——它不会被折叠成 0×0，而是会在菜单底部留一条可见的短线，所以必须删。

## 五、字体与焦点：本轮没有结论

D9 行里"字体""焦点"两项，本轮**没有**给出真机结论，原因不是没做，是做了也不能下结论：

- 全部用例跑在 `QT_QPA_PLATFORM=offscreen` 下。该平台**没有真实字体栈**，字体度量、字体回退、缺字方框这些现象在它下面测不出来；
- offscreen 下没有真实焦点窗口，Tab 顺序与焦点环只能验代码，验不了体验。

D7 已经用真机探针处理过字形覆盖（`docs/MEMBER_C_D7*`）。本轮不重复，也不把"桌面 PASS"写成"字体没问题"。

顺带记一条本轮踩到的测试陷阱，已写进代码注释与矩阵文档：**offscreen 下 `isVisible()` 对任何子控件都返回 false**（窗口从未真正显示）。判"控件是否被显式隐藏"必须用 `isHidden()`。第一版工具栏用例就是用它写的，注释里留了原因。

## 六、证据缺口：构建失败现在是留证的

D8 §五 留了一条边界：`scripts/dev/run-d3-tests.sh` 先 `rm -rf "$BUILD"`，`qmake`/`make` 裸跑在 `set -euo pipefail` 下，而保留日志的调用在其后——**构建一失败，失败现场什么都不剩**。这恰好是最需要证据的一类失败。

本轮补上（`scripts/dev/`）：

- qmake / make 分别捕获，失败不中止其余套件；
- 构建输出写 `build/d3-<suite>/build.log`，不再 `>/dev/null`；
- 三条此前静默的路径（qmake 失败 / make 失败 / 二进制未产出）都写结果文件并调用保留脚本，meta 里带 `stage=qmake|make|binary|test`；
- 保留脚本参数扩展为 `<suite> <status> <result> [<failed-stage> [<build-log>]]`，三参数旧用法完全不变，参数个数错误仍 `exit 2`。

`test_failure_log_retention.py` 从 13 项扩到 **24 项**，跑的是真实 runner（stub qmake/make 驱动，0.3 秒）。十个变异全部变红，其中两个是**测试抓出来、读代码没看出来的**：

- 所有"路径不可写"的旧用例都先让 results 的复制失败、在碰 build.log 之前就中止了，构建日志那两个写失败守卫**根本没有覆盖**；
- 重排循环后忘了把 `STAGE` 从 `make` 重置回 `test`，于是**普通的用例失败被记成 `stage=make / no test ran`**——与事实相反。

一处接口扩展要单独说明：新增 `D3_BUILD_ROOT`（默认值不变）。原因是新用例必须驱动真实 runner，而 runner 会对 `$BUILD` 做 `rm -rf`；不给覆盖点，测试就会删掉共享工作区里真实产物。

## 七、边界与未完成项

- **Windows 入口未改**：`scripts/Run-D3Tests.ps1` 仍只写 `test-results.txt`，无失败独立留证。本机**无法执行** PowerShell（`powershell.exe` 在 WSL 下 `Exec format error`），改了也无法验证，因此没有盲改。要全平台同等需另开一轮，由 CI（windows-2022）验证。
- **新代码路径只用 stub 驱动**：构建失败留证的三条路径没有用真 qmake/make 跑过。首次真实构建失败时会自然验证。
- **全部结论是宿主结论**，无真机取证。字体、焦点、真实 DPI 下的布局密度均未定论。
- **`archive` 套件那次未复现失败**：本轮全矩阵运行中未再出现（见第八节实测）。仍按 D8 的处理方式——不推测原因、不为未复现现象改生产代码。
- **#49（内置库失败文案把用户引向官网）未修**：根因在 `lc_application.cpp`，属组长维护的热点文件，本成员只报不改。
- 范围决策类问题（缩放 in/out 是否进 View 菜单、Time 工具栏是否干脆不建）留给组长，本轮按"最小改动修缺陷"处理。

## 八、本轮实测

基线：分支自 `origin/main`（`d9ff2a4f`，已含 #48）。修复前先跑满 10 套确认基线全绿，修复后重跑：

| 套件 | passed | failed | skipped |
| --- | --- | --- | --- |
| archive | 13 | 0 | 0 |
| priority | 8 | 0 | 0 |
| menu | **11** | 0 | 0 |
| modellist | 5 | 0 | 0 |
| panel | 15 | 0 | 0 |
| preferences | 9 | 0 | 0 |
| theme | 14 | 0 | 0 |
| chain | 9 | 0 | 0 |
| localization | 22 | 0 | 0 |
| files | 9 | 0 | 0 |
| **合计** | **115** | **0** | **0** |

`EXIT=0`。修复前 112 项，menu 套件新增 3 项后 115 项。

Python 静态检查：`test_failure_log_retention` 24 项、`test_network_scope` 10 项、`test_packaged_resources` 5 项、`test_export_part_list` 8 项，**47 项全通过**。

**`archive` 套件本轮 13/13 通过**，D8 那次未复现的失败没有再出现。

命令、原始日志与逐项结果见 [docs/evidence/member-c/2026-11-04/](./evidence/member-c/2026-11-04/)。

## 九、D9 阻断项与负责人

按方案文档的口径，D9 的统一退出条件是「阻断缺陷均有负责人和截止时间」：

| 阻断项 | 严重度 | 负责人 | 截止 | 状态 |
| --- | --- | --- | --- | --- |
| 空 Time 工具栏占位（本轮修复） | P2 | 成员 C | D9 当日 | **已修**，3 项用例 + 3 处变异锁定 |
| Windows 入口无失败留证（T-01） | P2 | 成员 C | 11/6 前 | 未修：本机无法执行 PowerShell，需 CI 验证 |
| 字体/焦点/布局密度无真机结论（T-02/T-03/T-04） | P1 | 组长 | 11/7 真机验收 | 待真机 |
| #45 LDR→MPD 无响应 | P1 | 成员 A | 待定 | 未关闭 |
| #49 内置库失败文案 | P2 | 组长 | 11/6 前 | 未修（热点文件） |
| #28 面板文案真机复验 | P2 | 组长 | 11/7 真机验收 | 代码已修，待复验 |
| #14 偏好裁剪真机复验 | P2 | 组长 | 11/7 真机验收 | 代码已修，待复验 |

截止时间一项：本轮能定的是 C 自己的两条（P2），其余依赖真机验收窗口与作者，**不由本成员单方宣布完成**。
