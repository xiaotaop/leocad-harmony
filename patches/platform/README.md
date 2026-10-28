# 平台层改动记录：离线启动与内置零件库接入

> 模块：`platform/`（HarmonyOS 应用与 Qt 平台集成）
> 记录人：成员 C　时间：2026-10-26
> 状态：**已直接入库**（原计划以补丁形式交组长，经组长确认后改为直接提交）

## 1. 为什么记录在这里

`patches/README.md` 的约定是：源码改动优先用正常 Git 提交，本目录用于记录
改动清单、理由与上游可合并性说明，不重复保存补丁文件。

本次改动因此直接提交，并在此记录：改了什么、为什么改、能否上游化。

## 2. 改动清单

| 文件 | 改动 | 原归属 |
|---|---|---|
| `qt/qtmain.cpp` | `#include "lc_library.h"`；把内置库注册进 `LibraryPaths` | 组长 |
| `common/lc_application.cpp` | 替换两处零件库失败文案 | 组长 |
| `resources/leocad_zh_CN.ts` | 同步两条新文案的中文译文 | 成员 C |

前两项按分工文档 §3.1 原属组长维护范围（应用入口、`lc_application.*`）。
经组长指示改为直接并入，故在此留档说明归属，便于追溯。

## 3. 改动理由

### 3.1 `qt/qtmain.cpp`：接入内置零件库

**问题**：`LibraryPaths` 只处理 Windows/Linux/macOS 分支，从未调用
`lcPiecesLibrary::FindBundledLibraryArchive()`。运行时会回落到
`LoadBuiltinPieces()`，只加载 `resources/library.zip` 里的 128 个零件，
不满足首发版「500–1500 个零件」的验收标准。

**两处细节都很关键**：

1. **必须显式 `#include "lc_library.h"`**。
   `lc_application.h` 只对 `lcPiecesLibrary` 做了前向声明（第 6 行），
   不加这行会编译失败。
2. **位置必须在 `LC_LDRAW_LIBRARY_PATH` 之后、`Initialize()` 之前**。
   - 放后面：保证用户显式配置的路径仍然优先；
   - 放前面：`Initialize()` 会消费整个列表，晚了不生效。
   - 第二个参数传 `true`：标记为官方件库，触发
     `lcPiecesLibrary::SetOfficialPieces()` 记录零件数。

**实测证据**（同一零件 `10172.dat`，它只存在于精简库）：

| 条件 | 库警告 | 渲染输出 |
|---|---|---|
| 强制内置库（`LEOCAD_LIB=/nonexistent`） | 有 | 2969 字节（空白） |
| 强制精简库（`LEOCAD_LIB=<zip>`） | 无 | 7439 字节 |
| 自动发现（本次改动） | 无 | 7439 字节 |

自动发现与强制精简库的输出字节一致，证明真的加载了 1160 件的库。

### 3.2 `lc_application.cpp`：修正失败文案

**问题**：原先两处文案都写着
「Please visit https://www.leocad.org for information on how to download and
install a library」。首发版零件库随包固定、不申请网络权限，让用户去网站下载
是**错误指引**——用户去了也装不上，且与 `docs/release/隐私政策.md` §2
「不发起网络请求」直接矛盾。

**改为**：「尝试重新安装应用」，这是用户实际能采取的动作。

| 场景 | 新文案 |
|---|---|
| 内置库也加载失败 | The built-in parts library could not be loaded, so only a limited set of parts is available. Try reinstalling the application. |
| 完全无法加载 | The parts library could not be loaded. Try reinstalling the application. |

中文：内置零件库加载失败，仅能使用少量零件。请尝试重新安装应用。
／零件库加载失败。请尝试重新安装应用。

## 4. 关于 `LC_DISABLE_UPDATE_CHECK`（本改动未包含）

原先准备过 `leocad.pro` 的 `ohos` qmake scope 来定义
`LC_DISABLE_UPDATE_CHECK=1`，以免启动时请求 `updates.txt` 而违反
「不申请网络权限」。核实组长分支后**已放弃**该改法：

`feature/leader-platform` 在 `CMakeLists.txt` 中已无条件定义：

```cmake
target_compile_definitions(leocad PRIVATE
    LC_DISABLE_UPDATE_CHECK=1
    LC_DISABLE_RENDER_DIALOG=1
    LC_DISABLE_BLENDER=1
)
```

鸿蒙构建走 CMake 而非 qmake，故 `.pro` 的改动属冗余。

**一处待确认**（不影响功能，仅提示差异）：上述定义是**无条件**的，
桌面 CMake 构建也会关闭更新检查。若本意只针对鸿蒙，可包在
`if(HARMONYOS) ... endif()` 内。桌面版是否保留更新检查属产品决策，
本记录不做判断。

## 5. 上游可合并性

| 改动 | 能否上游 | 说明 |
|---|---|---|
| `qtmain.cpp` 注册内置库 | **不宜**上游 | 依赖本项目新增的 `FindBundledLibraryArchive()` 与 `ldraw-lite/` 打包约定 |
| `lc_application.cpp` 文案 | **可以**上游 | 现有文案对任何离线分发场景都不合适；但上游默认联网，优先度低 |
| `ts` 译文 | 视上游语言维护流程 | 属正常翻译更新 |

## 6. 回退方式

三处互相独立，可分别回退：

| 只回退 | 结果 |
|---|---|
| `qtmain.cpp` | 运行时回落到 128 件内置库 |
| `lc_application.cpp` | 恢复指向下载页的旧文案 |
| `ts` 译文 | 两条新文案显示英文 |

整体回退：`git revert` 对应提交即可。

## 7. 相关记录

- 测试锁定：`tests/library/test_ui_scope.py::NetworkScopeTests`（宏缺口）
- 验收项：`docs/testing/测试矩阵.md` R-03、R-09
- 已知问题：`docs/release/已知问题.md` §5.1

## 8. 与其他分支的集成情况（已实测）

| 检查 | 结果 |
|---|---|
| `feature/leader-platform` 合并 `feature/c-ui-library` | 无冲突 |
| 组长 CMake 的 qrc 正则能否识别精简库条目 | 能（`alias` 与 `path` 均正确解析） |
| 合并后校验套件 | `RESULT: PASSED` |