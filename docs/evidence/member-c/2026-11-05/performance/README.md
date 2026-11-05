# D10 性能基线（成员 C，2026-11-05）

> 本目录只放**可复现的测量**与**原始输出**。结论写在 `docs/release/已知问题.md` 的「性能边界」一节，
> 两处数字必须一致；改一处要改另一处。

## 一句话结论

在 1160 件的精简库规模下，**零件索引与首启加载在桌面上不是瓶颈**：冷加载 13ms 量级，
逐击键过滤 0.27ms 量级。因此本轮**没有做任何优化**，也**没有**可报告的提速比例——
没有优化就没有百分比，写出来就是编造。

## 测了什么

| 项 | 方法 |
|---|---|
| 零件库加载（冷/热） | `lcApplication::LoadPartsLibrary()` 直接计时；冷=全新的空 `XDG_CACHE_HOME` |
| 窗口构造 | `new lcMainWindow()` + `CreateWidgets()` + `SetProject()` + `SetDefaultPart()` 分段计时 |
| 零件面板逐击键 | `PartsFilter` 的 `setText()` 端到端，含 `VisibleRowsChanged`→`RefreshStatus()` 的重计数 |
| 过滤成本随库规模的变化 | 用 `SetCustomParts()` 把同一份 `SetFilter()` 指向 1.16k / 5k / 20k / 50k / 100k 行 |
| 首启各阶段（含 GL 与 show） | 按 `lcApplication::Initialize()` 的真实顺序分段计时 |

## 怎么复现

```bash
source .devtools/env.sh
scripts/dev/perf-baseline.sh            # 默认 3 轮，输出到本目录 runs/
PERF_ROUNDS=5 scripts/dev/perf-baseline.sh
```

脚本会把 `perf-baseline.pro` / `perf-baseline.cpp` 复制到仓库根（`leocad.pro` 的相对路径只能从根解析），
在 `/tmp/bd-perf` 影子构建，跑完用 `trap` 删掉这两个临时文件。**不要把这两个文件提交到根目录**。

影子构建目录会在需要时自动创建，干净机器上首次运行不需要手工准备。目录**跨轮复用**（全量重建要
几分钟，而测量结果不依赖构建目录）；想换构建位置时可覆盖：

```bash
PERF_BUILD_DIR=/tmp/bd-perf-2 scripts/dev/perf-baseline.sh /tmp/perf-out-2
```

**每个 checkout 同时只能运行一个探针。** 不同 `PERF_BUILD_DIR` 和输出目录仍会共用仓库根的
两个暂存源码文件，一次运行退出时的 `trap` 会删除另一进程仍在使用的文件。需要并行时，必须
使用独立 checkout，并分别设置构建目录和输出目录；仅覆盖上面的目录不支持同 checkout 并行。

两轮测量之间要清掉缓存才有「冷」：脚本每轮都新建空的 `XDG_CACHE_HOME`，所以 `run-N` 之间互相独立。

## 文件

| 文件 | 内容 |
|---|---|
| `perf-baseline.cpp` / `.pro` | 探针源码。替换 `qt/qtmain.cpp` 的 `main()`，其余仍链接真实应用代码 |
| `perf-summarise.py` | 把 `KEY=value` 原始输出汇总成表格；不做平均，保留分布 |
| `runs/summary.md` | 生成的三轮汇总表 |
| `runs/run-N/core.txt`、`show.txt` | 每轮的原始输出，未加工 |
| `runs/run-N/*.err` | stderr；`show` 的重头是 `LEOCAD_GL_CONTEXT` JSON，记录宿主 GL 实际拿到了什么 |

## 环境

| 项 | 值 |
|---|---|
| 宿主 | WSL2，x86-64，Intel i9-13980HX（32 逻辑核），7.7 GB 内存 |
| Qt | 6.12.0（`.devtools/` 内发行包），`QT_QPA_PLATFORM=offscreen` |
| GL | Mesa 23.2.1，D3D12 后端，OpenGL 4.1 Compatibility |
| 零件库 | `assets/ldraw-lite/library-lite.zip`，3,833,541 字节，1160 件 |
| 构建 | debug（`CONFIG+=debug`），未开优化 |

**这是 debug 构建。** 因此所有数字偏保守（release 会更快），这正好说明不能拿它当上限或下限，
只能当「数量级判断」。

## 为什么这些数字不是立项结论

1. **offscreen 没有真实呈现。** `show()` 阶段实测 110ms（首次）/ 24ms（后续），
   和 `renderer_init_ms` 的 169-183ms 是同一类开销：Mesa 在 offscreen 表面上的初始化与合成，
   与真机 GPU 合成一个首帧的路径不同。它能说明「宿主上 show 不是零成本」，
   不能说明真机首帧是多少。

2. **I/O 与 CPU 完全不同，取库路径也不同。** 冷加载 13ms 是 x86 + SSD + 页面缓存已热的
   结果；鸿蒙真机是 ARM64 + UFS。更关键的是探针显式传入 zip 路径，而真机由
   `lcPiecesLibrary::FindBundledLibraryArchive()` 按候选顺序找库
   （`common/lc_library.cpp:260-283`：应用数据目录 → 二进制旁 → qrc 内嵌）。
   qrc 内嵌时首次访问要从 HAP 解压到内存，探针没走这条路。两者不能换算。

3. **debug 构建。** 见上，数字偏保守。

4. **1160 件的规模本身就是结论的一部分。** 这个规模下 `SetFilter()` 是 O(n)，而 n 很小，
   所以它快。它快不代表设计适合大库——增长曲线见下节。

## 实测数字（三轮，`runs/summary.md` 的摘要）

| 项 | 最小 | 中位 | 最大 | 样本 |
|---|---|---|---|---|
| 零件库冷加载 | 12.76 ms | 13.19 ms | 13.45 ms | 3（每轮 1 次） |
| 零件库热加载 | 4.77 ms | 5.33 ms | 6.53 ms | 15 |
| `new lcMainWindow()` + 快捷键 | 0.14 ms | 0.17 ms | 0.17 ms | 3 |
| `CreateWidgets()` | 19.50 ms | 20.28 ms | 25.02 ms | 3 |
| `SetProject()` | 4.56 ms | 4.73 ms | 5.15 ms | 3 |
| `SetDefaultPart()` | 0.73 ms | 0.74 ms | 0.89 ms | 3 |
| 逐击键（1161 行全量过滤） | 0.200 ms | 0.270 ms | 0.622 ms | 90 |
| `show()` 首次 | 110.3 ms | 111.6 ms | 117.3 ms | 3 |
| `show()` 之后各次 | 22.6 ms | 24.1 ms | 30.3 ms | 12 |

首启总时长（`show` 模式，含 GL 初始化）：

| 阶段 | 中位 |
|---|---|
| `lcContext::InitializeRenderer()` | 180.7 ms |
| 样式 + 窗口 + 库 + 控件 + 工程 | 41.2 ms |
| 以上合计 = `startup_before_show_ms` | 221.9 ms |
| `show()` 首次 | 111.6 ms |
| **合计（首次启动到首帧）** | **333.5 ms**（322.1-344.8） |
| （参考）`show()` 之后各次 | 24.1 ms |

注意两件事：

1. `renderer_init_ms` 占了首启的一半以上，而这一项在真机上是另一条完全不同的路径。
   把 333ms 当成「真机首启 333ms」是错的。
2. `startup_with_show_ms`（中位 482.8 ms）**不是**首启时间：探针在那里把 show() 连续做了
   五次，它是「启动 + 五次 hide/show」的累计值。首启只该算第一次 show()。
   第一版汇总用 `startup_with_show_ms` 当合计，多算了一百多毫秒；这里已更正。

## 过滤成本随库规模（合成，仅 1 轮）

`SetFilter()` 单次耗时，行数由 `SetCustomParts()` 撑出来（不是真实库，只测每行成本）：

| 行数 | `b` | `bri` | `brick` |
|---|---|---|---|
| 1,160 | 0.28 ms | 0.26 ms | 0.25 ms |
| 5,000 | 1.39 ms | 1.40 ms | 1.15 ms |
| 20,000 | 4.73 ms | 5.60 ms | 5.69 ms |
| 50,000 | 13.28 ms | 16.45 ms | 17.94 ms |
| 100,000 | 41.90 ms | 46.57 ms | 46.79 ms |

近似线性：1160 行时约 0.23 µs/行，10 万行时约 0.44 µs/行（每行成本随规模略升，缓存效应）。
与 `SetFilter()` 的实现一致：
`lc_partselectionwidget.cpp:297-322` 对**每一行**调 `mListView->setRowHidden()`，
而 `RefreshStatus()`（`lc_partselectionwidget.cpp:1670-1672`）紧接着又**扫一遍**所有行来数可见数。
每次击键两趟 O(n)。

**这条曲线的用途不是宣布一个缺陷。** 首发版是 1160 件，实测 0.27ms/击键，用户不可能感觉到。
它说明的是：如果将来换成完整 LDraw 库（本仓库 D4 记录的实测口径是 24,985 件，
见 `docs/D4_零件面板与库提示.md:173`），同一条路径会到 5-15ms/击键，那时才会有人抱怨。
这是**已知限制**，写进了发布说明。

## 本轮没有做的事

- 没有改任何生产代码。`common/` 一行未动。
- 没有「优化」。上面所有数字都是现状，不是改后的结果。
- 没有真机数字。一个都没有。
- 没有跑完整测试套件（`run-d3-tests.sh` 约 10 分钟且与其它成员抢内存）。
  探针本身不进测试矩阵，它不断言任何时间。

## 已知的两处实现细节（记录，不修）

1. **`SetFilter()` 的 `lcFilter` 与 `QRegularExpression` 无条件构造**（`lc_partselectionwidget.cpp:287-295`）。
   默认过滤类型下正则根本不用，但每次击键都会编译一次。在 1160 行、0.27ms 的量级下这不是问题，
   与上面的 O(n) 增长是两回事——但它也是「将来扩库时顺手要一起看」的地方。
2. **`RefreshStatus()` 的二次全表扫描**（`lc_partselectionwidget.cpp:1670-1672`）。
   可见数本来可以在 `SetFilter()` 的循环里顺带数出来。同样是 O(n)，同样在 1160 行下无关紧要。

两条都不在本任务范围内修：本任务明确要求不改生产代码，且这两处改动会与权限任务、回归验证互相干扰。
已报告组长，由组长决定是否单开一项。
