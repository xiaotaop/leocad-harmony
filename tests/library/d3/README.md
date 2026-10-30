# D3/D4 接入测试

五个套件，覆盖启动时的库发现与校验、库优先级、菜单裁剪、子模型对话框和零件面板状态。
共 44 项断言。D4 的 `panel` 套件源文件在 `tests/library/d4/`，工程名不带 `d3` 前缀。

| 套件 | 工程 | 项数 | 覆盖 |
|---|---|---:|---|
| archive | `d3-archive-tests.pro` | 13 | `ValidateBundledLibraryArchive()` 的四项检查 |
| priority | `d3-priority-tests.pro` | 8 | `PlaceBundledLibraryArchive()` 的排序与拒绝 |
| menu | `d3-menu-tests.pro` | 8 | `ApplyFirstReleaseScope()`、子模型标签栏与空子菜单 |
| modellist | `d3-modellist-tests.pro` | 5 | `lcModelListDialog` 只保留切换 |
| panel | `d4-panel-tests.pro` | 10 | 零件面板计数、空态覆盖层与库异常横幅 |

## 为什么需要这些测试

**归档校验**：`lcPiecesLibrary::Load()` 对不可用的归档**返回 true**——
`OpenArchive()` 跳过所有不以 `ldraw/` 开头的条目后仍无条件 `return true`。因此
「启动成功」本身不能证明校验函数有效，必须用已知的坏归档去撞它。

**库优先级**：`LoadPartsLibrary()` 在第一个加载成功的路径上立即返回，而鸿蒙入口
先把 `:/resources/library.zip`（128 件）注册进列表。精简库若用 `+=` 追加，鸿蒙包
会静默选中 128 件库。

这一条**桌面端抓不到**：`:/resources/library.zip` 只在 `#ifdef Q_OS_HARMONY` 下
注册，桌面构建里精简库是唯一候选，`+=` 与 `prepend` 的渲染结果 SHA-256 完全相同。
所以测试必须直接调用 `PlaceBundledLibraryArchive()`——这个函数本来在入口文件里，
是为了能被测试调用才移进 `lc_library` 的。

初版 priority 测试没有这样做：它在测试内部重建了一份路径列表，再断言自己刚构造
的顺序，从未执行入口文件的真实代码。把 `prepend` 改回 `+=` 复跑仍 5/5 通过。

## 用例

### archive

| 用例 | 变体 | 期望 |
|---|---|---|
| 完整归档 | 原始 `library-lite.zip` | 通过，件数 == manifest 的 `counts.parts` |
| 根目录错误 | `wrongroot.zip` | 拒绝，提示 `outside ldraw/` |
| 缺色表 | `nocfg.zip` | 拒绝，提示 `ldconfig` |
| 色表为空 | `emptycfg.zip` | 拒绝，提示 `is empty` |
| 色表无颜色指令 | `garbagecfg.zip` | 拒绝，提示 `no usable colour definitions` |
| 有指令无数据 | `brokencolour.zip` | 拒绝，同上 |
| 有 CODE 无 VALUE | `codeonlycfg.zip` | 拒绝，同上 |
| 引用断裂 | `dangling.zip` | 拒绝，提示 `references a file` |
| 截断 | `truncated.zip` | 拒绝，提示 `expected` |
| 文件不存在 | — | 拒绝，提示 `not found` |
| 校验不污染全局色表 | — | `gColorList`、`gDefaultColor`、`gEdgeColor` 不变 |

变体由 `prepare_variants.py` 从当前发布归档生成，不入库：归档变了，变体跟着变，
不会过期。

### priority

| 用例 | 期望 |
|---|---|
| 精简库排在 128 件库之前 | 列表首项是精简库 |
| 加载后件数 | 1160 |
| 按名核对 `10172` 与 `3001` | `10172` 仅精简库有 |
| 排在所有平台条目之前 | 含 `LC_LDRAW_LIBRARY_PATH` 之类的条目也在其后 |
| 校验失败不进列表 | 列表原样返回，问题串非空 |
| 归档不存在不进列表 | 问题串为 `archive not found` |
| 内置归档结构 | 128 件 + 8 图元 |

最后一项限制写清楚：内置 `resources/library.zip` **没有 `ldconfig.ldr`**，过不了
`ValidateBundledLibraryArchive()`（那是精简库的契约），但 `LoadColors()` 会退到
`lcLoadDefaultColors()`，加载不受影响。计数用例因此直接读 zip 结构，不借用校验器。

## 运行

```bash
source .devtools/env.sh          # 或确保 qmake 在 PATH
JOBS=2 scripts/dev/run-d3-tests.sh            # 五个套件全跑
JOBS=2 scripts/dev/run-d3-tests.sh priority    # 只跑一个
JOBS=2 scripts/dev/run-d3-tests.sh panel       # D4 面板套件
```

`JOBS=2` 而不是更高：GCC 11 在内存压力下会报 `could not split insn` 或
`Terminated signal terminated program cc1plus`，与代码无关，降并发即可通过。

Windows / CI 用 `scripts/Run-D3Tests.ps1`。脚本会先生成变体，再逐个 shadow build，
任何套件打印 `FAIL!` 或退出非零都会让整体失败。

手工跑单个套件：

```bash
mkdir -p build/d3-priority && cd build/d3-priority
qmake ../../d3-priority-tests.pro CONFIG+=debug && make -j4
QT_QPA_PLATFORM=offscreen ./build/debug/leocad-d3-priority
```

`menu`、`modellist` 和 `panel` 要构造真实窗口，没有显示器时必须设 `QT_QPA_PLATFORM=offscreen`。

## 回归验证

每个关键断言都实际改坏过并确认会失败，否则不知道测试是否在测东西：

| 改坏的行为 | 失败项数 |
|---|---:|
| `prepend` 改回 `+=` | 3 |
| 校验失败仍把路径塞进列表 | 2 |
| 颜色只校验存在不校验内容 | 4 |
| 颜色改用 `!COLOUR` 前缀计数 | 4 |
| 忽略 `AllowStructureEditing` | 2 |
| 摘空子菜单的容器（`Import`/`Export`/`Step` 变回可见空框） | 1 |
| 去掉构造末尾的 All Parts 选择 | 2 |
| `SetLibraryProblem()` 提前返回 | 5 |
| 空态忽略 `mLibraryProblem`（库损坏时仍提示"没有匹配的零件"） | 1 |
| 横幅不走调色板（去掉 `setPalette`） | 1 |

最后一条第一次改坏时**没有失败**：库正常时列表非空，`VisibleCount == 0` 不成立，
那段代码根本没执行到。用例补上"先搜索到空、再设库异常"之后才真正生效。
