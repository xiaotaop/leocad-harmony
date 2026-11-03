# 成员 C D8：首发界面与包内资源收口

日期 2026-11-03。基线 main `b947ef80`（含 A #46、B #47、组长 #44 与 C #43 合并后的状态）。
D8 要求为「完成全部首发界面和包内资源；停止新增页面」。本轮**不新增任何页面或功能**，
做四件事：逐项核对首发界面已无越界入口；把源码中每个硬编码 URL 与每个资源引用拿去判定可达性；
修掉核对中发现的**两处**缺口并分别锁定；修掉取证工具自身的两处缺陷。

两处缺口分别是：帮助菜单两项会拉起浏览器（§一），以及两个菜单命令引用了不存在的图标文件
（§二之二）。二者性质不同——前者违反离线承诺，后者是长期无人察觉的资源死引用——
但都属「首发界面与包内资源是否真的完成」这一问，都不是新增功能。

## 一、本轮修掉的越界入口：帮助菜单会拉起浏览器

界面冻结核对时发现的真实缺口，也是本轮唯一的生产代码改动。

`common/lc_mainwindow.cpp` 的帮助菜单里有「主页」和「报告问题」两项，分别调用
`QDesktopServices::openUrl` 打开 `https://www.leocad.org/` 和 GitHub issues。

**为什么 `LC_DISABLE_NETWORK` 拦不住它**：该宏只把 `lc_http` 换成空实现（见
`common/lc_http.cpp:4`、`lc_http.h:3`），而 `QDesktopServices` 属 Qt GUI，不在其覆盖范围。
结果是应用自身不发请求，却会拉起浏览器去联网——与 `隐私政策.md` §2「不发起网络请求」、
`权限说明.md` §2.3 的离线承诺直接冲突。此前这两项**没有任何拦截**，也不在任何裁剪列表里
（`LC_HELP_UPDATES` 被裁剪了，但主页与报告问题没有）。

修复：两项加入 `ApplyFirstReleaseScope()` 的 `RemovedCommands[]`，与该函数已有的裁剪方式一致
（不另设编译分支，避免"哪个宏管哪一项"再次被误读）。

**双重锁定**：

- 静态检查 `scripts/library/test_network_scope.py` 新增两项断言：这两条命令在裁剪列表内；且
  `lc_mainwindow.cpp` 里 `openUrl(http…)` 的调用点数必须等于裁剪命令数——新增一处外链调用而
  忘了入列会直接失败，而不是等复查发现。
- 窗口级测试 `tests/library/d3/tst_menu_scope.cpp` 把两条命令加进 `removedCommandsAreNotVisible()`，
  在真实 `lcMainWindow` 的菜单上确认它们不可见。

**负向验证**（两处都做了）：从 `RemovedCommands[]` 删掉这两行后，
`test_external_url_commands_are_trimmed` 失败、菜单套件 `7 passed / 1 failed`；恢复后
`10 passed` / `8 passed` 全绿。

文档同步：`隐私政策.md` §2 与 `权限说明.md` §2.3 各补一段，说明「没有网络权限」不等于
「没有任何联网动作」，这两层必须分别核对——`LC_DISABLE_NETWORK` 管前者，菜单裁剪管后者。

## 二、包内资源核对

| 项 | 结果 |
|---|---|
| qrc 条目 | 合计 **164**：`leocad.qrc` 156（其中 8 条为 `.qm` 构建产物）+ `resources/stylesheet/stylesheet.qrc` 8。**仓库需提供的 156 条全部存在** |
| 翻译 | 仓库跟踪 **8 个 `.ts`**；`.qm` **不在仓库里**（`.gitignore:15` 的 `*.qm`），由构建生成。`zh_CN` 编译结果 **1891 条，0 条未完成** |
| 离线零件库 | `assets/ldraw-lite/library-lite.zip` **3833541 字节**，SHA-256 `09922e82…4268c0f3`，与 `manifest.json` 记录**逐字节一致** |
| 库清单 | 1160 零件 / 669 子零件 / 911 基元 / 1 配置，共 **2741 文件**，`files` 数组 2741 条 |
| 应用图标 | `tools/icon/` 下 64/256/512 三档均被跟踪，经 alias 暴露为 `:/resources/leocad.png`、`-256`、`-512`；窗口图标在 `lc_mainwindow.cpp:87` 按这三档取用 |
| 零件库入包方式 | `library-lite.zip` 通过第 4 个 alias 打进 qrc，运行期由 `lc_library.cpp:280` 从 `:/assets/ldraw-lite/library-lite.zip` 读取——**这是内置库在设备上可用的实际机制**，此前无记录 |
| 着色器 | **17 个** `.glsl` 全部存在：`resources/shaders/` 下 fakelit 4、unlit 12（含 view_sphere 一对）、text_sdf 1。磁盘与 qrc 双向比对，无「有文件未打包」也无「有条目缺文件」 |
| 代码资源引用 | `common/` + `qt/` 中 **145 处**唯一 `:/…` 引用全部可解析（按检查器口径：剥离注释后统计；修复前为 147 处，其中 2 处不可解析，见 §二之二） |

**一条本可在本地发现、却只能由 CI 抓出的错误**：第一版核对写的是「156 条 qrc 条目逐条
`os.path.exists`，缺失 0 条」，本地全绿而 CI 的 port-policy 直接失败——因为 `.qm` 是构建产物、
根本不在仓库里，本地那份是早前 `lrelease` 留下的**陈旧生成文件**。换句话说，那条断言当时测的是
我这台机器的目录状态，不是仓库状态。现在检查显式区分**仓库提供**与**构建生成**两类资源：
前者必须存在，后者必须能追溯到被跟踪的源文件（8 个 `.ts`），并新增
`test_generated_translations_have_tracked_sources` 锁定这条追溯关系。

验证覆盖两种检出状态：**移走全部 `.qm` 模拟干净检出后 5 项仍 OK**；抽走 `leocad_ru.ts` 后立即以
`['leocad_ru.qm has no leocad_ru.ts']` 失败，恢复后通过。教训本身：**凡按文件存在性做的核对，
都要问一句「干净检出会不会长成这样」**，否则检查器会被本地残留喂出假绿。

`.qm` 在 CMake 构建时由 `qt_add_translations()` 生成，`CMakeLists.txt:30-52` 会把 `leocad.qrc` 中
的 `.qm` 条目过滤掉再生成构建树 qrc（避免与生成的翻译资源重复），所以 `.qm` 实际来自构建产物；
qrc 里保留条目是给 qmake 路径用的。这一处此前无记录，本轮核实并写在这里。

### 二之二、引用不存在文件的图标：缩放 in/out

资源审计（不只数 qrc 条目，而是把代码里每个 `:/…` 引用拿去和 qrc 比对）查出 2 处死引用。
审计口径要说明：计数**剥离注释后**进行，修复前 147 处、修复后 145 处——若不剥离注释，
本轮为解释此缺陷而写进代码的一句 `":/..."` 会被自己算成引用（与下文「检查器一度被注释骗倒」是同一件事）：
`common/lc_mainwindow.cpp` 对 `LC_VIEW_ZOOM_IN` / `LC_VIEW_ZOOM_OUT` 调用
`setIcon(QIcon(":/resources/view_zoomin.png"))` 与 `view_zoomout.png`，**这两个文件不存在、也从未存在于本仓库**
（`git log --all --diff-filter=A -- "*view_zoomin*" "*view_zoomout*"` 无结果，非遗失）。Qt 对缺失资源不报错，
只返回 null 图标，因此这类引用可以长期存在而不被发现。

**影响面经核查为零**，这点要说清楚，不能夸大成「界面坏了」：这两个命令**既不在任何菜单里，也不在任何工具栏里**
（`ViewMenu` 只加 `LC_VIEW_ZOOM_EXTENTS`；`mStandardToolBar` / `mTimeToolBar` / `mToolsToolBar` 均无缩放项），
所以图标从来没有被渲染过。它们的可达途径只有键盘：`lc_commands.cpp` 表里 `DefaultShortcut` 为 `+` / `-`，
经 `lc_shortcuts.cpp:61` 全表装载、`lc_mainwindow.cpp:2691` 赋给动作，而动作在 `CreateActions()` 里
对所有命令 id 统一 `addAction()` 挂到窗口上。命令处理分支仍在 `lc_mainwindow.cpp:3474/3479`。

处理方式：**移除这两处死引用，而不是凭空补两张图标**。D8 是冻结日，「完成包内资源」不应成为塞入新美术的理由；
且这两个命令没有任何可见界面，补图标在今天没有可验证的收益。若日后要把缩放暴露进菜单，
新检查会要求**先有真实文件、再谈引用**。

顺带记一条给组长的首发界面观察（**本轮不擅自改**，属范围决策）：缩放 in/out 目前是**纯键盘命令**，
界面上无处可点、无处可发现。对 PC 键盘用户可用，但真机上若有不带 `+/-` 的输入场景就不可达。
是否加入 View 菜单按分工文档 §1.2 末尾的「等量交换」规则由组长定，不应在冻结日由本成员单方新增入口。

新增 `scripts/library/test_packaged_resources.py`（**5 项**，已接入 `continuous.yml` 的 port-policy）：

| 用例 | 锁住什么 |
|---|---|
| `test_every_qrc_entry_is_either_tracked_or_generated` | qrc 列出的条目要么在仓库里，要么属于已声明的构建产物（`.qm`），两个 qrc 的相对根不同 |
| `test_generated_translations_have_tracked_sources` | 每个 `.qm` 都能追溯到被跟踪的 `.ts`，防止构建静默停止产出某个语言 |
| `test_code_references_resolve_to_packaged_resources` | 代码中每个 `:/…` 引用都能解析到已打包资源；按各自 `prefix` 计算真实资源路径，并对动态拼接名重试扩展名 |
| `test_bundled_library_matches_its_manifest` | `library-lite.zip` 的字节数与 SHA-256 与 manifest 一致 |
| `test_library_manifest_counts_match_the_file_list` | manifest 的 `counts` 与 `files` 列表自洽 |

两处踩过的坑已写进代码注释，因为它们决定这个检查能否被信任：

- 起初忽略 `<qresource prefix="/stylesheet">`，把 `close.svg`、`stylesheet.qss` 判成缺失——**误报会淹没真问题**，
  资源路径必须按各自 prefix 计算。
- 解释性注释里写的 `":/..."` 字样被当成引用，导致检查因散文而失败。**能被注释骗过的检查会被禁用**，
  因此改为在剥离注释之后再扫描（同时跟踪字符串字面量，避免 URL 里的 `//` 误删整行），偏移与行号保持不变。

**负向验证**：把 `view_zoomin.png` 那行恢复后，检查立即以
`{'/resources/view_zoomin.png': ['common/lc_mainwindow.cpp:313']}` 失败并点名行号；移除后 4 项 OK。

## 三、首发界面：已做到的裁剪

`ApplyFirstReleaseScope()` 现移除 **57 条命令**（本轮 +2；统计已扣除注释中提到的宏名，按 `RemovedCommands[]` 数组内真实条目计）。分段如下，与分工文档 §1.2
「首发版明确不做」逐条对应：

| 条数 | 分组 | 对应 §1.2 条目 |
|---|---|---|
| 4 | 打印、HTML 说明书、导出图片 | 打印、HTML 说明书 |
| 10 | 非核心导入导出格式（3DS/COLLADA/HTML/CSV/POV-Ray/Wavefront/BrickLink/LDD/清单等） | POV-Ray、Blender 等外部渲染或导出；LXF/OBJ/3DS/COLLADA 等 |
| 3 | 外部渲染器（POV-Ray、Blender、在 Blender 中打开） | 同上 |
| 10 | 步骤编辑与时间轴导航 | 动画、复杂多模型步骤编辑 |
| 7 | 时间轴右键菜单的第二套命令 id | 同上 |
| 7 | 查找替换 | 原版帮助内容与高级选项 |
| 4 | 多视图布局 | 多窗口和多视图 |
| 9 | 子模型结构编辑（切换保留 `LC_MODEL_LIST`） | 复杂多模型步骤编辑 |
| 2 | **外部网址两项（本轮新增）** | 联网功能 |
| 1 | 在线更新检查 | 在线更新 |
| **57** | 合计 | |

菜单裁剪与 `LC_DISABLE_*` 宏是两层独立保护：宏关掉的是代码路径（`lc_http`、更新检查、
渲染对话框、Blender），裁剪关掉的是用户可达的入口。两层都成立才谈得上首发范围收敛，
所以本轮新增的静态检查同时覆盖两侧。

### 偏好对话框的界面范围

菜单之外，`视图→选项` 对话框同样属首发界面。D4 真机报告的五个越界项在
`qt/lc_qpreferencesdialog.cpp` 的 `FirstReleaseHiddenWidgets[]` 中已全部覆盖
（`checkForUpdates`、POV-Ray 三件套、LGEO 两件套、外部零件库三件套、`BlenderAddonSettingsButton`）。
清单里每个控件名都经 `Q_ASSERT_X` 校验可解析——`.ui` 改名会在测试期直接断言失败，
而不是静默漏掉一个本该隐藏的选项；宿主 `preferences` 套件 9 项全绿。真机复验仍待组长，见 #14。

## 三之二、全部可达网址的穷举审计

修完帮助菜单后不能只相信那两条，因此把源码里所有硬编码 URL 逐条判定可达性。方法与总数一并写出，便于复核：

```
grep -rn "https\?://" --include=*.cpp --include=*.h common/ qt/   →  18 处
按文件：project.cpp 4 / lc_blenderpreferences.cpp 4 / lc_setsdatabasedialog.cpp 3 /
        lc_updatedialog.cpp 2 / lc_mainwindow.cpp 2 / lc_application.cpp 2 / lc_filter.cpp 1
```

下表覆盖这 18 处的全部判定，另加 `lc_blenderpreferences.cpp:1844` 一处
`openUrl("file:///…")`：它不匹配 `http(s)://` 因而不在 18 处之内，但同为 `openUrl` 调用，一并判定。
（`lc_filter.cpp:115` 是 Apache 许可证头注释，不计入行为。）

| 位置 | 内容 | 首发版是否可达 | 依据 |
|---|---|---|---|
| `lc_mainwindow.cpp:3665,3669` | `openUrl` 打开 leocad.org / GitHub issues | **否（本轮修复）** | 加入 `RemovedCommands[]`，菜单与静态检查双重锁定 |
| `lc_updatedialog.cpp:42,122` | 下载 `updates.txt`、releases 链接 | 否 | `LC_DISABLE_UPDATE_CHECK=1` + `LC_HELP_UPDATES` 已裁剪 |
| `lc_setsdatabasedialog.cpp:21,113,174` | Rebrickable API | 否 | `LC_DISABLE_NETWORK=1` 使 `lc_http` 编译为空 + `LC_FILE_IMPORT_INVENTORY` 等已裁剪 |
| `project.cpp:1860,1882,1987` | 导出 HTML 里写入 `leocad.org` 锚点 | 否 | `LC_FILE_INSTRUCTIONS`、`LC_FILE_EXPORT_HTML` 均在裁剪列表内，`ShowHTMLDialog()` 唯一调用点是后者 |
| `lc_blenderpreferences.cpp:1844` | `openUrl("file:///" + LogFile)`（本地文件，非网络） | 否 | 调用链经 `LC_DISABLE_BLENDER=1` 编译为空，且该按钮 `BlenderAddonSettingsButton` 在 `FirstReleaseHiddenWidgets` 中已隐藏——**双重保护，不会留下点击无响应的死控件** |
| `lc_blenderpreferences.cpp:34,35` | Blender 附加组件仓库与 GitHub API 地址（下载用） | 否 | 仅经 `lc_http` 使用，被 `LC_DISABLE_NETWORK=1` 编译为空；其入口命令亦已裁剪 |
| `lc_blenderpreferences.cpp:99,102` | 路径提示文字 `(https://library.ldraw.org)`、`(bricklink.com/.../download.page)` | 否 | Blender/Stud.io 偏好页的静态说明文本，纯文本不可点击，该页在首发版隐藏 |
| `project.cpp:1480` | COLLADA XML 命名空间 `http://www.collada.org/...` | 否 | 命名空间标识符，写入导出文件而非访问；COLLADA 导出本身已裁剪 |
| `lc_application.cpp:1428,1430` | 零件库加载失败提示正文含 `https://www.leocad.org` | 提示本身可达，**但为纯文本、无 `<a>`，不可点击，不触发任何联网动作** | `QMessageBox::information` 纯文本 |
| `lc_filter.cpp:115` | Apache 2.0 许可证头注释 | 否 | 注释 |

结论：**修复后这 19 处中不存在任何用户可触达、会发起联网动作的界面入口**——两处修复覆盖的 2 处，
其余 17 处分别由「命令已裁剪」「代码编译为空」「控件已隐藏」「非网络文本」四类原因排除，
多数同时受两层保护。

唯一残留问题属于文案而非行为，且不在本成员可改文件内：`lc_application.cpp` 那两条提示在鸿蒙首发版语境下
不成立——零件库是**打包内置**的（`assets/ldraw-lite/library-lite.zip`），引导用户「去网站下载并安装库」
既不适用也无法在沙箱内完成。若真机出现该提示，实际含义是内置库加载失败，应按缺陷排查而不是让用户去装库。
建议在 D9 文案轮调整为内置库故障措辞（该文件属组长热点，本成员未改动）。

## 四、测试结果

本轮在更新后的 `main`（`b947ef80`）上完整跑了一遍：

```
archive 13 / priority 8 / menu 8 / modellist 5 / panel 15 / preferences 9 /
theme 14 / chain 9 / localization 22 / files 9
```

**10 个套件共 112 项，全部通过，0 失败 0 跳过。** Python 静态检查 **23 项**通过
（`test_network_scope.py` 10 项 + `test_export_part_list.py` 8 项 + `test_packaged_resources.py` 5 项），
`git diff --check` 干净。

改动前的完整回归为 112 项全绿，改动后第一遍同样全绿（第二遍见下节）；Windows CI 上
`MenuScopeTest::removedCommandsAreNotVisible()` 为 PASS，套件 8 passed / 0 failed。

### 一次未复现的失败，以及证据为什么丢了

改动后的完整回归跑了两遍：**第一遍 112 项全绿，第二遍 `archive` 报出 `Totals: 12 passed, 1 failed`**，
其余套件正常。针对这个失败重新采样：单独运行 `archive` 1 次（13 passed）、换用下面修正后的 runner 再跑
完整回归 1 次（archive 13 passed）、直接循环 `leocad-d3-archive` 二进制 15 次（每次 13 passed）——
**共 17 次采样全部通过，未能复现**。**因此不宣称该失败已定位或已修复**，也不据此改生产代码。

真正暴露出来的是取证链断在工具上：`scripts/dev/run-d3-tests.sh` 用
`... | tee "$BUILD/test-results.txt" | tail -3` 回显结果，而 QtTest 把 `Totals` 打在最后一行、
`FAIL!` 打在它上面——三行刚好只够显示 `Loc:`、`cleanupTestCase` 和 `Totals`，**失败用例名与原因
被截掉**。同时脚本每次 `rm -rf "$BUILD"` 重建目录，唯一的完整结果副本随之消失，下一次运行就已无法追查。
这与 D7 在 CI 侧踩过的坑（`*>` 产生 0 字节结果文件）是同一类问题：**套件报红但说不出哪一条红，
等于没有证据**。

已修 runner：改为完整写入结果文件、按 `^(FAIL!|QFATAL|Totals)` 回显（点名失败用例与原因），
并把每套件结果复制到 `build/last-run/`——该目录不在循环的删除范围内，重跑之后仍能回看上一次失败。

## 五、边界与未完成项

- `archive` 套件出现过一次未复现的失败，原因**未知**（见 §四）。留待 D9 若再次出现时按新日志定位，
  本轮不推测、不为此改生产代码。
- 本轮是**宿主验证**，不是真机。帮助菜单裁剪在真机上的表现（图标、菜单宽度）未另行取证。
- `localization` 套件里的字形覆盖与文字宽度两项，在 Windows CI 上因 runner 无字体库而 `SKIP`
  （D7 已记录），本轮不改这一状况；**字体结论仍以真机探针为准**。
- 缩放 in/out 是**纯键盘命令**（`+` / `-`），首发界面里没有任何可点入口。是否加进 View 菜单属范围决策，
  按「等量交换」由组长定；本成员未在冻结日单方新增入口（详见 §二之二）。
- 首发界面「全部完成」是**裁剪意义上的完成**：范围内功能可用、越界入口已移除。不包含
  外观层面的真机逐页验收，那属 D9 真机回归。
- 开放问题未关闭：`#28`（面板文案；省略已在 D7 代码侧修复并有两条宿主 PASS 的断言，
  缺的是真机复验——见 §五 末与本节下条的教训）、`#14`（偏好设置裁剪）、
  `#15`（系统文件选择闭环）、`#13`（启动前模态）、`#16`（Qt 默认权限）、`#45`（A 的无响应跟踪）。
  其中 `#14`、`#15` 与首发界面相关，但按分工文档属后续阶段或组长范围，本轮不擅自收敛。
- `module.json5` 是否声明 `ohos.permission.INTERNET` 仍待组长在打包产物上核对（源码宏替代不了）。
- **本轮自我复核抓出的一处推断错误**：我在 #28 上先写了「文字省略仍未修」，随后核对代码与测试发现
  该修复早在 D7 就已落地（`lc_partselectionwidget.cpp:1405` 设最小宽度，`tst_localization.cpp` 两条断言
  在宿主真实 PASS），已另发评论更正并在文档里改正表述。错误来源是把 D7 文档中「某一类宽度断言被移除」
  直接推成「功能没实现」——**文档讲的是断言取舍，实现是另一回事**。凡下结论必须回到代码与可运行的检查。
- 上述只能由真机定论的项目（帮助菜单外观、文字省略、字体可用性、内置库加载、缩放可发现性）
  已整理成交接清单：[MEMBER_C_D8_DEVICE_HANDOFF.md](MEMBER_C_D8_DEVICE_HANDOFF.md)。本成员未做任何真机结论。

## 六、停止新增页面

自本轮起不再添加任何首发界面页面或功能。后续工作限定为：修复已记录的缺陷、完善测试矩阵
（D9 范围）、以及发布材料填写。任何需要新增页面的需求都应先与现有范围交换，按分工文档 §1.2
末尾的规则处理。
