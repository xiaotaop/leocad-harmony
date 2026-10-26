# 补丁：鸿蒙离线化与内置零件库接入

> 提交者：成员 C（作为修改建议）
> 目标文件：`qt/qtmain.cpp`、`common/lc_application.cpp`、`resources/leocad_zh_CN.ts`
> 归属依据：分工文档 §3.1 —— 这些文件由组长维护，其他成员**通过小型独立 PR 提交修改建议**
> 补丁文件：`patches/platform/0001-harmonyos-offline-and-bundled-library.patch`

## 1. 为什么要走补丁而不是直接提交

分工文档 §3.1 规定：

- `qt/qtmain.cpp` 的平台入口改动归组长
- 根构建文件（`leocad.pro`）归组长
- `lc_application.*` 归组长

同一节又要求「其他人通过小型独立 PR 提交修改建议」。因此本目录存放可直接应用的补丁，
由组长复核后合入。成员 C 不直接改这三个文件。

## 2. 补丁内容

> **重要**：其中「`LC_DISABLE_UPDATE_CHECK`」一项经核实组长已在 CMakeLists.txt 完成，
> 补丁不再包含（见 §2.1）。补丁实际包含 2 处代码改动 + 1 处译文同步。

### 2.1 【已完成，补丁不再包含】`LC_DISABLE_UPDATE_CHECK`

**状态更正（2026-10-26，核实组长分支后）**：组长已在
`feature/leader-platform` 的 `CMakeLists.txt` 中无条件定义：

```cmake
target_compile_definitions(leocad PRIVATE
    LC_DISABLE_UPDATE_CHECK=1
    LC_DISABLE_RENDER_DIALOG=1
    LC_DISABLE_BLENDER=1
)
```

因此本补丁**不再包含** `leocad.pro` 的改动——那会变成重复定义。
原先准备的 `ohos` qmake scope 仅在用 qmake 构建鸿蒙版时才有意义，
而当前鸿蒙构建走 CMake，属冗余。

**需要组长确认的一点**：这三个宏目前是**无条件**定义的，
意味着桌面 CMake 构建也会关闭更新检查。若本意只针对鸿蒙，可改为：

```cmake
if(HARMONYOS)
    target_compile_definitions(leocad PRIVATE
        LC_DISABLE_UPDATE_CHECK=1
        LC_DISABLE_RENDER_DIALOG=1
        LC_DISABLE_BLENDER=1
    )
endif()
```

桌面版是否保留更新检查属产品决策，成员 C 不做判断，仅提示差异。

### 2.2 【P0】`qt/qtmain.cpp`：接入内置零件库

**问题**：`LibraryPaths` 只处理 Windows/Linux/macOS，从未调用
`FindBundledLibraryArchive()`。结果是运行时回落到 `LoadBuiltinPieces()`，
只加载 `resources/library.zip` 里的 **128 个零件**，不满足首发版
「500–1500 个零件」的验收标准。

**做法**：
1. `#include "lc_library.h"` —— **必须显式包含**。
   `lc_application.h` 只对 `lcPiecesLibrary` 做了前向声明（第 6 行），
   不加这行会编译失败。
2. 在 `LC_LDRAW_LIBRARY_PATH` 之后注册内置库。
   位置很关键：
   - 放在 `LC_LDRAW_LIBRARY_PATH` **之后**，保证显式配置的路径仍优先；
   - 放在 `Initialize()` **之前**，因为该调用会消费整个列表。
   - 第二个参数传 `true` 标记为官方件库，触发 `SetOfficialPieces()` 记录零件数。

**已验证**（对照实验，同一零件 `10172.dat`）：

| 条件 | 警告 | 渲染输出 |
|---|---|---|
| 强制内置库（`LEOCAD_LIB=/nonexistent`） | 有 | 2969 字节（空白） |
| 强制精简库（`LEOCAD_LIB=<zip>`） | 无 | 7439 字节（有几何） |
| **自动发现（本补丁）** | **无** | **7439 字节** |

自动发现与强制精简库的渲染输出**字节完全一致**，证明真的加载了 1160 件的精简库。

### 2.3 【P1】`lc_application.cpp`：修正零件库失败文案

**问题**：现有两处文案都写着
「Please visit https://www.leocad.org for information on how to download and install a library」。
首发版零件库随包固定、不申请网络权限，让用户去网站下载是**错误指引**——
用户去了也装不上，且与「完全离线」的承诺矛盾。

**做法**：改为「尝试重新安装应用」，这是用户实际能采取的动作。

| 场景 | 新文案 |
|---|---|
| 内置库也加载失败 | The built-in parts library could not be loaded, so only a limited set of parts is available. Try reinstalling the application. |
| 完全无法加载 | The parts library could not be loaded. Try reinstalling the application. |

对应中文已同步进 `resources/leocad_zh_CN.ts`：

- 内置零件库加载失败，仅能使用少量零件。请尝试重新安装应用。
- 零件库加载失败。请尝试重新安装应用。

## 3. 应用方式

```bash
cd /path/to/leocad-harmony
git checkout -b feature/leader-harmony-offline
git apply patches/platform/0001-harmonyos-offline-and-bundled-library.patch
```

若已有本地改动导致冲突，可用三方合并：

```bash
git apply --3way patches/platform/0001-harmonyos-offline-and-bundled-library.patch
```

## 4. 合入后请验证

```bash
# 1. 桌面构建仍正常
scripts/dev/build-desktop.sh

# 2. 零件库自动发现（关键）
source .devtools/env.sh && export QT_QPA_PLATFORM=xcb
./build/desktop/build/debug/leocad --image /tmp/x.png --width 200 --height 150 <任一含非常用零件的.ldr>
# 期望：不出现 "could not find a compatible Parts Library"

# 3. 宏在鸿蒙构建生效
grep -o "\-DLC_DISABLE_UPDATE_CHECK=1" <鸿蒙构建目录>/Makefile
# 期望：有输出

# 4. 二进制中不应再有更新检查 URL（鸿蒙构建）
strings <鸿蒙构建目录>/leocad | grep -c "updates.txt"
# 期望：0
```

第 4 条是最可靠的判据：宏生效时 `lcDoInitialUpdateCheck()` 的调用点被预处理器移除，
`updates.txt` 字符串不会进二进制。

## 5. 测试联动

`tests/library/test_ui_scope.py::NetworkScopeTests::test_macro_is_not_yet_defined_for_harmonyos`
当前是 **skip** 状态，它会统计 `leocad.pro` 中 `LC_DISABLE_UPDATE_CHECK=1` 的出现次数：

- 现在 = 1（只在 macx 段）→ skip 并提示缺口
- 合入本补丁后 = 2 → 自动转为强断言通过

所以合入补丁后**不需要再改测试**，它自己会变绿。

## 6. 未包含的项

**中文语言回退**（原先列为第 3 项）经实测**不需要做**：

| 传入 tag | `QLocale::name()` | 加载 `leocad_<name>.qm` |
|---|---|---|
| `zh_CN` | `zh_CN` | ✅ |
| `zh-Hans` | `zh_CN` | ✅ |
| `zh-Hans-CN` | `zh_CN` | ✅ |
| `zh` | `zh_CN` | ✅ |
| `zh_SG` | `zh_SG` | ❌ |

Qt 已把简体中文的各种写法规范化为 `zh_CN`，现有代码直接命中。
只有新加坡简体（`zh_SG`）会失败，属可接受降级（界面回落英文，不影响功能）。
如果后续确实需要，再单独处理。

## 7. 回退方式

```bash
git apply -R patches/platform/0001-harmonyos-offline-and-bundled-library.patch
```

三处改动互相独立，也可只回退其中一处：

- 只回退 2.1 → 鸿蒙构建恢复联网检查（不推荐）
- 只回退 2.2 → 运行时回落到 128 件内置库
- 只回退 2.3 → 恢复指向下载页的旧文案

## 8. 补丁内容清单（实际文件）

| 文件 | 改动 | 归属 |
|---|---|---|
| `qt/qtmain.cpp` | `#include "lc_library.h"`；注册内置库到 `LibraryPaths` | 组长 |
| `common/lc_application.cpp` | 替换两处零件库失败文案 | 组长 |
| `resources/leocad_zh_CN.ts` | 同步两条新文案的中文译文 | 成员 C |

原先准备的 `leocad.pro` `ohos` scope 已移除：组长在 `CMakeLists.txt` 中
无条件定义了同样的宏，保留会造成重复（见 §2.1）。

`resources/leocad_zh_CN.ts` 一并放进补丁是必要的：新文案只在源码改动后存在，
若译文单独留在成员 C 的分支，会造成 ts 与源码不一致（`check_translations.py`
会报 stale 项）。因此源码改动与对应译文必须同批合入。

`scripts/l10n/fill_zh_cn.py` 中的对应译文条目已在成员 C 分支单独提交，
用于后续重新生成 ts 时保持幂等。

## 9. 与其他分支的集成情况（已实测）

| 检查 | 结果 |
|---|---|
| 补丁应用到 `feature/c-ui-library`（HEAD） | ✅ 干净 |
| `feature/leader-platform` 合并 `feature/c-ui-library` | ✅ 无冲突 |
| 补丁应用到上述合并结果 | ✅ 干净 |
| 合并后的校验套件 | `RESULT: PASSED` |
| 组长 CMake 的 qrc 正则能否识别精简库条目 | ✅ 能（`alias` 与 `path` 均正确解析） |

**应用顺序**：若先合并 `feature/c-ui-library` 再应用本补丁，`ts` 部分可干净应用；
直接应用到未合并的 `feature/leader-platform` 时，`ts` 一节会因基线不同而冲突
（该分支的 ts 尚未重新生成）。因此建议：**先合并成员 C 分支，再应用补丁**。
