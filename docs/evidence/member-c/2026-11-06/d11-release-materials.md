# D11 证据：发布材料冻结的验证记录

> 成员 C，2026-11-06。基线 `bbcbfa7a`，分支 `feature/c-d11-release-materials`。
> 本文只记录**实际跑过**的命令与输出。没跑过的不写。

## 1. Python 静态检查（7 个脚本，62 项）

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
| `verify_sample_model.py` | 3 | 0 |
| **合计** | **62** | **0** |

D11 新增 7 项（network_scope +3、release_materials +4），另有 verify_sample_model 3 项独立脚本。

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

## 6. 未跑项（如实列出）

| 项 | 原因 |
|---|---|
| 10 套 QTest 全量回归 | 已单独在后台执行，结果见 §7；即便通过也只覆盖桌面（offscreen），不改真机结论 |
| Windows 侧 `scripts/Run-D3Tests.ps1` | 本机无法执行，只在 CI（windows-2022）验证 |
| 真机验证 | 无设备，归组长 RC0/RC1 |
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
