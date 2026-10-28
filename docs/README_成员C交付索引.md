# 成员 C 交付索引

> 负责人：成员 C　范围：界面、精简零件库、汉化、发布与质量材料
> 依据：`docs/LeoCAD_鸿蒙PC首发版_两周分工与技术方案.md` §3、§3.1、§4、§9

本文件是成员 C 全部交付物的入口。每条都指向可直接核对的产物或验证方法。

## 1. 交付物总览

| 板块 | 分工文档要求 | 产物 | 状态 |
|---|---|---|---|
| 界面精简 | 主窗口去除非本期菜单、偏好设置裁剪 | `common/lc_mainwindow.cpp` 的 `ApplyFirstReleaseScope()`；`qt/lc_qpreferencesdialog.cpp` 的 `RemovedPreferenceWidgets[]` | 已实现 + 桌面验证 |
| 零件面板 | 分类/搜索/颜色/空态 | `common/lc_partselectionwidget.*`（空态为视口覆盖层） | 已实现 |
| 精简零件库 | 500–1500 件、可追溯、可离线 | `assets/ldraw-lite/library-lite.zip`（1160 件）+ `manifest.json` | 已生成 + 六项校验通过 |
| 库发现 | 包内资源路径 | `lcPiecesLibrary::FindBundledLibraryArchive()` + `leocad.qrc` 条目 | 已实现 |
| 汉化 | 中文资源、无乱码 | `resources/leocad_zh_CN.ts`（2056 条，必需 context 全覆盖） | 已重新生成 + 运行时验证 |
| 颜色中文 | 显示层翻译、不污染导出 | `lcGetColorDisplayName()` + `resources/color_names_zh.tsv` | 已实现 |
| 测试 | 库清单检查、界面范围、偏好设置裁剪 | `tests/library/`（66 项） | 全绿 |
| 发布材料 | 商店/隐私/权限/第三方/源码 | `docs/release/`（6 份） | 初稿完成 |
| 组长交接 | 离线化与库接入 | `patches/platform/0001-*.patch` | 已出补丁，可直接 `git apply` |
| 质量材料 | 测试矩阵、已知问题 | `docs/testing/`、`docs/release/已知问题.md` | 完成 |

## 2. 代码改动清单

| 文件 | 改动 | 可回退性 |
|---|---|---|
| `common/lc_library.h/.cpp` | 新增 `FindBundledLibraryArchive()` / `GetBundledLibraryArchiveCandidates()` | 纯新增，不影响既有 `Load()` |
| `common/lc_mainwindow.h/.cpp` | 新增 `ApplyFirstReleaseScope()`；窗口几何恢复加屏幕校验；颜色按钮用显示名 | 不调用该函数即恢复上游菜单 |
| `common/lc_category.h/.cpp` | 新增 `lcGetCategoryDisplayName()` 与翻译抽取入口 | 纯新增 |
| `common/lc_colors.h/.cpp` | 新增 `lcGetColorDisplayName()` 与翻译抽取入口；提示框改用显示名 | 纯新增 |
| `common/lc_partselectionwidget.h/.cpp` | 空态提示 `mEmptyStateLabel`、`UpdateEmptyState()`；分类名走显示层 | 空态可隐藏 |
| `qt/lc_qpropertiesdialog.cpp` | 颜色标签走显示名 | 单行改动 |
| `qt/lc_qpreferencesdialog.cpp` | 偏好设置裁剪：`RemovedPreferenceWidgets[]` 隐藏 19 个越界控件 + 6 个标签（不改 `.ui`） | 删该数组即恢复完整偏好设置 |
| `leocad.qrc` | 新增精简库资源条目（仅新增一行） | 删该行即回退 |
| `resources/leocad_zh_CN.ts` | `lupdate` 重生成 + 补译 | 可回退到 `git checkout` |
| `resources/color_names_zh.tsv` | 新增 52 条颜色对照 | 纯新增 |

**未修改的共享热点文件**（按分工文档 §3.1 交组长）：
`qt/qtmain.cpp`、`lc_application.cpp`、`leocad.pro`、根 CMake。
需要组长的 4 项改动见 §5。

## 3. 文档索引

| 文档 | 内容 |
|---|---|
| `docs/ui/界面清单与精简范围.md` | 菜单/偏好设置逐项对照、精简机制、降级方式 |
| `docs/ui/术语与翻译对照.md` | 术语表、颜色对照、空态文案、维护流程 |
| `docs/library/零件库清单与许可.md` | 1160 件的来源、筛选规则、分类分布、CC BY 4.0 五项署名、可复现性 |
| `docs/testing/测试矩阵.md` | 21 项自动化 + 7 项桌面 + 18 项真机验收 |
| `docs/release/商店文案.md` | 简介、详述、关键词、截图方案、一致性核对 |
| `docs/release/隐私政策.md` | 不收集数据、离线、权限最小化 |
| `docs/release/权限说明.md` | 逐项权限、**更新检查联网 P0** |
| `docs/release/第三方组件清单.md` | Qt/zlib/LDraw 许可、**GPLv2 与 Qt 兼容性待裁定** |
| `docs/release/源码获取说明.md` | GPLv2 义务自检、待补仓库信息 |
| `docs/release/已知问题.md` | 范围限制、未译项、待真机项、已修缺陷留档 |

## 4. 脚本与测试

| 脚本 | 用途 | 需要网络 |
|---|---|---|
| `scripts/dev/install-qt-desktop.sh` | 装 Qt 6.12 到 `.devtools/`（无需 sudo） | 是（首次） |
| `scripts/dev/build-desktop.sh` | 桌面构建（默认 `JOBS=4`，防换页） | 否 |
| `scripts/dev/verify_menu_scope.py` | 运行时截屏核对菜单范围 | 否 |
| `scripts/library/fetch_ldraw.py` | 取官方库到仓库外缓存 | 是 |
| `scripts/library/generate_selection.py` | 按 `lc_category.cpp` 生成种子清单 | 否 |
| `scripts/library/build_library.py` | 打包 + 写 manifest（可复现） | 否 |
| `scripts/library/verify_library.py` | 六项校验 | 否 |
| `scripts/library/check_qrc_resources.py` | qrc 资源存在性 | 否 |
| `scripts/l10n/check_translations.py` | 翻译门禁 | 否 |
| `scripts/l10n/fill_zh_cn.py` | 补齐译文（幂等） | 否 |
| `scripts/ci/run_library_checks.sh` | 汇总全部检查 | 否 |

```bash
# 一条命令跑完成员 C 的所有检查
scripts/ci/run_library_checks.sh
```

## 5. 需组长合入的改动

**已整理为可直接应用的补丁**：`patches/platform/0001-harmonyos-offline-and-bundled-library.patch`
（配套说明：`patches/platform/README.md`）

```bash
cd /path/to/leocad-harmony
git apply patches/platform/0001-harmonyos-offline-and-bundled-library.patch
```

补丁包含 3 项（第 4 项经实测不需要做，见 §5.3）。成员 C 只提供补丁，不直接修改这些文件。

### 5.1 【P0】鸿蒙构建必须定义 `LC_DISABLE_UPDATE_CHECK=1`

- **位置**：`leocad.pro`（或在 CMake 中等价位置）
- **现状**：该宏只在 `macx` 段定义（第 166 行）
- **后果**：鸿蒙构建不定义则首次启动会请求 `https://www.leocad.org/updates.txt`，
  违反"不申请网络权限"，无网络时报错
- **证据**：`qt/qtmain.cpp:222` → `common/lc_updatedialog.cpp:42`
- **锁定测试**：`tests/library/test_ui_scope.py::NetworkScopeTests`（当前 skip）
- **详见**：`docs/release/权限说明.md` §3.1、`docs/release/已知问题.md` §5.1

### 5.2 库发现接入应用入口

- **位置**：`qt/qtmain.cpp` 的 `LibraryPaths`
- **建议**：在 `LC_LDRAW_LIBRARY_PATH` 之后加入
  ```cpp
  QString BundledLibrary = lcPiecesLibrary::FindBundledLibraryArchive();
  if (!BundledLibrary.isEmpty())
      LibraryPaths += qMakePair(BundledLibrary, true);
  ```
- **为什么**：`FindBundledLibraryArchive()` 已实现且校验通过，但入口未调用它，
  所以首版仍会只加载 128 件内置库
- **降级**：即使不接入，`leocad.qrc` 已把精简库编进二进制，
  可通过 `LEOCAD_LIB` 或应用数据目录覆盖

### 5.3 中文语言回退 —— 实测不需要做

- **位置**：`qt/qtmain.cpp` 翻译加载处
- **建议**：`Translator.load("leocad_" + Locale.name())` 失败且系统语言以 `zh` 开头时，
  重试 `leocad_zh_CN`
- **为什么**：鸿蒙可能上报 `zh-Hans` 而非 `zh_CN`，直接按名称拼会找不到 `.qm`

### 5.4 零件库缺失提示细分

- **位置**：`lc_application.cpp` 约 968–987 行
- **建议**：把现有单一提示拆为"未找到"/"加载失败"两种文案
- **为什么**：成员 C 已在零件面板加了空态提示，启动弹窗也应能区分

## 6. 验证命令

```bash
# 全部静态与单元检查
scripts/ci/run_library_checks.sh

# 桌面构建（需先跑 install-qt-desktop.sh）
scripts/dev/build-desktop.sh

# 启动稳定性（各 3 次应全部 exit=124，即被 timeout 正常结束）
source .devtools/env.sh && export QT_QPA_PLATFORM=xcb
for i in 1 2 3; do
  LEOCAD_LIB=$PWD/assets/ldraw-lite/library-lite.zip timeout 6 \
    ./build/desktop/build/debug/leocad >/dev/null 2>&1; echo -n "$? "
done
```

## 7. 当前状态与遗留

| 项 | 状态 |
|---|---|
| 66 项单元测试 | 全绿（1 项 skip，用于跟踪 5.1 的宏缺口） |
| 桌面构建 | 76 个翻译单元通过 |
| 启动稳定性 | 带库 5/5、无库 3/3 正常 |
| 精简库生效实证 | 通过（用只在精简库中的零件对比渲染） |
| 中文运行时生效 | 通过（独立 `QTranslator` 探针） |
| 真机验证 | **待组长**（18 项验收，见测试矩阵 §3） |
| 组长改动 | **已出补丁，待合入**（`patches/platform/`） |
| GPLv2/Qt 兼容性裁定 | **待法务** |
| 商店文案定稿与截图 | **待组长**（D11–D12） |
