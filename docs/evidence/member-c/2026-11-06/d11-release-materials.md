# D11 证据：发布材料冻结的验证记录

> 成员 C，2026-11-06。基线 `bbcbfa7a`，分支 `feature/c-d11-release-materials`。
> 本文只记录**实际跑过**的命令与输出。没跑过的不写。

## 1. Python 静态检查（7 个脚本，63 项）

```bash
for f in scripts/library/test_*.py scripts/library/verify_*.py; do python3 "$f"; done
```

| 脚本 | 项数 | exit |
|---|---:|---:|
| `test_export_part_list.py` | 8 | 0 |
| `test_failure_log_retention.py` | 24 | 0 |
| `test_network_scope.py` | 13 | 0 |
| `test_packaged_resources.py` | 5 | 0 |
| `test_permission_scope.py` | 5 | 0 |
| `test_release_materials.py` | 4 | 0 |
| `verify_sample_model.py` | 6 | 0 |
| **合计** | **63** | **0** |

D11 新增 11 项：network_scope +3、release_materials +4、verify_sample_model +4（含几何校验与子零件解析）。

## 2. 变异验证（证明检查不是空转）

每项都在真实文件上制造回归，确认脚本**非零退出**，然后还原并确认恢复通过。

### 2.1 `test_release_materials.py`

| 变异 | 操作 | 结果 |
|---|---|---|
| M1 图标被换 | 向 `tools/icon/64x64/apps/leocad.png` 追加 1 字节 | exit=1，1 项失败 |
| M2 商店身份不符 | 把文案中 "LeoCAD Harmony" 全改为 "LeoCad Harmony" | exit=1，1 项失败 |
| M3 重新引入被禁解释 | 向第三方清单追加"可以作为 or later 处理，补写声明即可。" | exit=1，1 项失败 |
| M4 承诺不存在的脚本 | 向源码说明追加"可用 scripts/library/build_library.py 重建同一档案。" | exit=1，1 项失败 |
| 还原 | 全部恢复原文件 | exit=0，0 失败 |

### 2.2 `verify_sample_model.py`

| 变异 | 操作 | 结果 |
|---|---|---|
| M1 零件不在精简库 | `3001.dat` → `30011.dat` | exit=1 |
| M2 未定义色码 | 色码 `320` → `999` | exit=1 |
| 还原 | 恢复原文件 | exit=0 |

### 2.3 `test_network_scope.py`

| 变异 | 操作 | 结果 |
|---|---|---|
| About 外链改回可点击 | `openExternalLinks` false → true | exit=1，失败信息指名 `common/lc_aboutdialog.ui` |
| 还原 | 恢复 | exit=0 |

## 3. qmake 编译与菜单套件（`.ui` 改动必须真编译）

```bash
source .devtools/env.sh
bash scripts/dev/run-d3-tests.sh menu
```

输出：`Totals: 11 passed, 0 failed, 0 skipped, 0 blacklisted, 49ms`，exit 0。

这一步的意义：`.ui` 改动若 XML 非法或 property 名写错，uic 会失败，套件编不出来。
仅跑 Python 检查无法证明这一点。

## 4. 图标哈希独立复核

```bash
cd tools/icon && find . -type f | while read f; do sha256sum "$f"; done
```

16/16 与 `docs/release/图标冻结.md` 表格逐行一致，含 512px 的
`a784800d4e8c87bfd2c6d92956dd6494ce38fd71d81d03d738f8e33c87a86140`（173,848 字节）。

## 5. 外部链可达性核查（`.ui` 层面）

| 文件 | 链接目标 | 可达性 | 依据 |
|---|---|---|---|
| `common/lc_aboutdialog.ui` | `https://www.leocad.org` | **可达** | `LC_HELP_ABOUT` 不在 `ApplyFirstReleaseScope()` 裁剪列表；`lc_mainwindow.cpp:633` 加入帮助菜单 |
| `common/lc_setsdatabasedialog.ui` | `https://www.rebrickable.com` | **可达** | `lc_partselectionwidget.cpp:1290` 右键菜单"编辑调色板"→ `lc_partpalettedialog.cpp:131` `ImportButtonClicked()` → `lcSetsDatabaseDialog` |
| `common/lc_updatedialog.ui` | （运行时填充） | **不可达** | `LC_HELP_UPDATES` 在裁剪列表；`lcDoInitialUpdateCheck()` 在 `#if !LC_DISABLE_UPDATE_CHECK` 内，而 `CMakeLists.txt:77` 无条件定义该宏为 1 |

前两处已改为纯文本（`openExternalLinks`=false）；第三处保持原样，并由新增检查断言其两个不可达前提仍成立。

网络层方面：`common/lc_http.cpp` 在 `LC_DISABLE_NETWORK` 下把 `lcHttpManager::DownloadFile()` 桩成立即发
`DownloadFinished` 且 `error()` 恒为 true，因此这两处对话框的**搜索功能本身不发请求**；
本轮修的是"点击拉起浏览器"这一个真实副作用。


## 5.1 组长复核后的修正（a07b5be → 本轮）

组长在 PR #62 提两项 P2，两项均已复现并修正。

### P2-1：截图模型的竖直层级方向反了

**组长判断成立，且我的原检查完全看不到它。**

实测依据：`common/lc_math.h` 的 `lcVector3LDrawToLeoCAD()` 是 `(x, y, z) → (x, z, -y)`；
从 `assets/ldraw-lite/library-lite.zip` 里直接量出零件自身几何：3001/3003/3004 的 y 范围为
`[-4, 24]`（stud 占 -4..0，本体 0..24）——**原点在顶面，本体向 +Y 延伸，即 Y 越大越低**。
原模型把屋顶放在 y=72、烟囱放在 y=96/120，等于全部堆在墙体**下方**。

修正：按零件真实尺寸重排竖直层级（往上堆 = y 递减）：

| 层 | 顶面 y | 本体区间 |
|---|---:|---|
| 底板 2×（4×8 板） | 0 | 0..8 |
| 墙第一层 | -24 | -24..0 |
| 墙第二层 | -48 | -48..-24 |
| 屋顶 2×（4×8 板） | -56 | -56..-48 |
| 烟囱 ×2 | -80 / -104 | -80..-56、-104..-80 |

同时把墙改为 1×2 砖拼砌，并让前后墙跨 x=-60..60、四角归侧墙，避免四面墙在角落交叉。

### P2-2：验证器静默跳过无法匹配的 type-1 行

**组长给出的复现完全成立，已修。**

原因：正则只接受普通小数，不接受 `0e0` 这种科学计数（LDraw 合法，Python `float()` 不认），
且两个循环都只在 `match` 成功时才纳入检查。把第一条零件行改成
`1 999 0e0 0 0 1 0 0 0 1 0 0 0 1 nonexistent-part.dat` 后，脚本仍输出 3 tests OK、退出 0。

修正：改为逐条解析。

- 每个 type-1 行必须恰好 15 个字段，否则记入 `unparsed` 并失败；
- 数值先经 `NUMBER` 校验再 `float()`，显式支持指数形式；
- 非 1–5 的语句也不再被忽略；
- 新增 `test_every_type_one_line_is_parseable`，任何无法解析的行直接使测试失败。

### 新增的几何检查（组长指出的盲区）

组长明确指出"零件/色码存在性检查无法发现该几何问题"。因此新增
`test_the_model_stacks_upward`：从库内零件的自有几何（递归读 .dat 的型号 3/4 顶点与子件变换）
算出每个零件的 y 区间，应用该行的位置与旋转，再从地面向上扫描要求材料连续。
断开的层会按零件名与行号报出。


### 追加发现：子零件引用因反斜杠被静默跳过（同类缺陷）

在按组长要求做"正常视图确认"时，我写了一个离屏预览渲染器，结果第一版只画出两块灰色平板，
墙和屋顶全部缺失。追查后确认这是**与组长指出的 P2-2 同类的静默跳过**：

- LDraw 的 .dat 文件内部引用子零件用**反斜杠**（`s\3001s01.dat`），归档里是正斜杠；
- 我的 `find_part()` 用 `"/" + part` 匹配，于是所有子零件都找不到，被静默跳过；
- 后果不只是渲染错：**几何检查的 y 区间也因此偏小**，因为零件的 stud 与很多轮廓都在子零件里。

修正：`find_part()` 先把反斜杠归一为 `/` 再匹配；同时 `_extent()` 遇到无法解析的子零件不再 continue，
而是记入 `UNRESOLVED`，并新增 `test_part_geometry_resolves_completely` 对整个模型断言该列表为空。
现在共 **6 项**检查。

### 正常视图确认的做法与局限

本机**没有 cmake**（`cmake: command not found`），仓库也没有预构建的 `leocad` 可执行文件，
因此无法用 LeoCAD 自己的渲染器出图。改为两条互补的确认：

1. **几何数值**：从归档内零件的真实顶点递归算出世界坐标包围盒：
   x -80..80、y -104..8、z -80..80（LDraw 空间，y 向下），并按层列出每个零件的 y 区间，
   确认底板→墙一层→墙二层→屋顶→烟囱是连续且单调的。
2. **正投影立面图**：新脚本 `scripts/library/render_sample_preview.py` 用同样的真实顶点数据
   画正视与侧视两个正投影（正投影没有深度排序，画出来就是几何本身），输出
   `docs/evidence/member-c/2026-11-06/screenshot-sample.preview.png`。
   图上自上而下为：棕色烟囱 → 深红屋顶 → 黄色二层墙 → 红色一层墙 → 灰蓝底板。

**这不等于真机确认。** 它证明的是"按零件真实尺寸堆叠，层级方向正确"，不证明真机渲染效果、
颜色观感或构图合适——后者仍归组长 RC1 真机验收。预览图仅作为冻结前的自检依据，不作为商店素材。

### 修正后的变异矩阵（7/7 捕获）

| 变异 | 结果 |
|---|---|
| 组长原复现：`0e0` + 未知零件 + 无效色码 | exit=1 |
| 仅未知零件（语法合法） | exit=1 |
| 仅未定义色码 | exit=1 |
| 截断的 type-1（14 字段） | exit=1 |
| **屋顶移回 y=+72（原缺陷）** | **exit=1** |
| **烟囱移到墙下方** | **exit=1** |
| **一层墙被抬离（出现间隙）** | **exit=1** |
| 还原 | exit=0 |

其中“仅未知零件”一项在初版变异脚本里因构造错误显示为漏检，按精确替换重测为 exit=1。

## 6. 未跑项（如实列出）

| 项 | 原因 |
|---|---|
| 10 套 QTest 全量回归 | 已单独在后台执行，结果见 §7；即便通过也只覆盖桌面（offscreen），不改真机结论 |
| Windows 侧 `scripts/Run-D3Tests.ps1` | 本机无法执行，只在 CI（windows-2022）验证 |
| 真机验证 | 无设备，归组长 RC0/RC1 |
| LeoCAD 自身的离屏渲染（`-i/--image`） | 本机无 cmake、也无预构建的 leocad 可执行文件，无法跑 |
| RC0 release 签名包核对 | 包不存在（`packaged-module.json` 仍 `debug=true`） |
| 截图 | D12，`[待拍]` |

## 7. 全量回归（10 套 QTest）

```bash
source .devtools/env.sh
bash scripts/dev/run-d3-tests.sh
```

| 套件 | 结果 | 用时 |
|---|---|---:|
| archive | 13 passed, 0 failed | 213ms |
| priority | 8 passed, 0 failed | 186ms |
| menu | 11 passed, 0 failed | 34ms |
| modellist | 5 passed, 0 failed | 11ms |
| panel | 15 passed, 0 failed | 865ms |
| preferences | 9 passed, 0 failed | 10ms |
| theme | 14 passed, 0 failed | 8ms |
| chain | 9 passed, 0 failed | 53ms |
| localization | 22 passed, 0 failed | 74ms |
| files | 9 passed, 0 failed | 4ms |
| **合计** | **115 passed, 0 failed, 0 skipped, 0 blacklisted** | — |

与 `docs/D9_测试矩阵.md` §二记载的 115 项一致，无新增失败。

**这 115 项仍然只覆盖桌面（offscreen）**，不改变任何真机结论——真机项依旧归组长（§6）。
