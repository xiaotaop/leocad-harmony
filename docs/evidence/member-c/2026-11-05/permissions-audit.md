# D10 权限核对记录（成员 C）

日期：2026-11-05　执行者：成员 C（助手代执行）
任务：`task-4`「D10 权限说明：按打包产物重写，6 项实际权限逐项说明」

## 1. 核对方法（可复现）

| 步骤 | 命令 / 动作 | 目的 |
|---|---|---|
| 1 | `cat docs/evidence/leader/2026-11-02/d7-device/packaged-module.json5` | 取打包产物的真实清单 |
| 2 | `grep -c '"name": "ohos.permission' <同上>` → **6** | 数目核对 |
| 3 | `grep -c 'ohos.permission.INTERNET' <同上>` → **0**；`GET_NETWORK_INFO` → **0** | 确认网络权限不在清单 |
| 4 | `grep -rn 'requestPermissions' scripts/ qt/ common/ CMakeLists.txt` → 无命中 | 证明本仓库不生成清单 |
| 5 | `sed -n '100,140p' CMakeLists.txt` | 找到清单的真正来源：Qt 打包模板 |
| 6 | `grep -rn 'clipboard' common/*.cpp common/*.h`（另加大写 `Clipboard`） | 剪贴板链路 |
| 7 | `grep -rn 'QFileIconProvider'`、`'QIconProvider'`、`'fileIcon'`、`'FileIcon'` → 无命中 | `GET_FILE_ICON` 无使用点 |
| 8 | `grep -rn 'QFileSystemModel'`、`'QDirModel'`、`'QTreeView'` → 无命中 | 首发版无文件图标 UI |
| 9 | `grep -rn 'LC_FILE_PRINT'`、`'ShowPrintDialog'`、`'QPrintDialog'`、`'QPrinter'` | 打印可达性 |
| 10 | `grep -rho 'ohos.permission.[A-Z_]*' docs/evidence/leader/ --include='*.hilog.txt'` 后排序去重 | 设备日志里实际出现的权限（**只有 READ_PASTEBOARD，80 次**） |
| 11 | `gh issue view 16 --json ...` | 取 issue 的方法论与范围（普通 view 被 Projects classic 弃用报错挡住） |

**全流程是静态核对。没有在任何真机上执行过权限拒绝/取消路径的测试。**

## 2. 清单的真正来源（本次核对最重要的结构性发现）

`CMakeLists.txt:107-135`：

```
111  get_filename_component(_harmony_qt_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
112  set(_harmony_template "${_harmony_qt_prefix}/src/harmonyos/templates")
113  if(NOT EXISTS "${_harmony_template}/entry/src/main/module.json5")
114      message(FATAL_ERROR "Missing Qt HarmonyOS package template: ${_harmony_template}")
115  endif()
116  set(_harmony_package "${CMAKE_CURRENT_BINARY_DIR}/leocad-harmony-package")
117  file(COPY "${_harmony_template}/" DESTINATION "${_harmony_package}")
```

即：**清单 = Qt 安装目录里 `src/harmonyos/templates` 的一份副本**，本仓库只往里塞图标
（`:118-119`）和 `QT_HARMONYOS_APP_*` 属性（`:120-134`），**权限一行都没碰**。

两个直接后果：

1. 想在仓库里"改权限"是**改不动的**——要么改 DevEco 工程，要么改 Qt 模板副本；
2. `CMakeLists.txt:117` 每次配置都重新 `file(COPY)`，**在 build 目录里手改 `module.json5`
   会在下一次 configure 时被覆盖**，不能作为交付方式。

## 3. 逐项代码依据（文件:行）

| # | 权限 | 应用侧使用点 | 结论 |
|---|---|---|---|
| 1 | `STORE_PERSISTENT_DATA` | `common/lc_profile.cpp:181-258`（`QSettings` 读写）；`common/lc_mainwindow.cpp:49-71`（最近文件加载/保存） | 有用，但"该权限是否为其前提"未查证 |
| 2 | `FILE_ACCESS_PERSIST` | `common/lc_mainwindow.cpp:2803`（`getOpenFileName`）、`:2938`（`getSaveFileName`） | 必要 |
| 3 | `PREPARE_APP_TERMINATE` | **无**（`common/`、`qt/` 下 0 命中）；关闭走 `common/lc_mainwindow.cpp:1316` `closeEvent` | 用途未证实 |
| 4 | `READ_PASTEBOARD` | `common/lc_mainwindow.cpp:132-133`（dataChanged 连接）、`:1452-1462`（`ClipboardChanged`）、`:2510-2517`（`UpdatePaste`）；`common/lc_application.cpp:766-780`（`SetClipboard`/`ExportClipboard`）；`common/lc_model.cpp:1180-1200`（`Copy`/`Paste`） | **必要** |
| 5 | `GET_FILE_ICON` | **无**（0 命中） | 无用，建议移除 |
| 6 | `PRINT` | 命令移除 `common/lc_mainwindow.cpp:658-659`；动作禁用 `:781-782`；快捷键为空 `common/lc_commands.cpp:145,152`；处理函数 `:1716` 仅由 `:3133-3134` 调；唯一入口 `:1472` 由 `sender()` 驱动 | 不可达，建议移除 |

## 4. `READ_PASTEBOARD` 拒权路径的空/失败处理（逐行核对）

| 位置 | 代码 | 结论 |
|---|---|---|
| `common/lc_mainwindow.cpp:1455-1459` | `const QMimeData* MimeData = QApplication::clipboard()->mimeData();` 之后 `if (MimeData && MimeData->hasFormat(MimeType))` | **判空**，读不到剪贴板不解引用 |
| `common/lc_application.cpp:766-770` | `SetClipboard()` 存空数组后 `UpdatePaste(!mClipboard.isEmpty())` | 空内容 → 粘贴置灰 |
| `common/lc_mainwindow.cpp:2510-2517` | `UpdatePaste()` 对 `LC_EDIT_PASTE`/`LC_EDIT_PASTE_STEPS` 判空指针后 `setEnabled` | 不会崩 |
| `common/lc_model.cpp:1190-1193` | `Paste()` 开头 `if (gApplication->mClipboard.isEmpty()) return;` | 空内容短路 |
| `common/lc_application.cpp:772-780` | `ExportClipboard()` 只调 `setMimeData`（写），不读系统剪贴板 | **复制不依赖读权限** |

设备侧佐证：`docs/evidence/leader/2026-11-02/d7-device/final-first-session.hilog.txt:47`
（该文件内共 3 处，全 leader 证据里 80 处）都有
`ohos.permission.READ_PASTEBOARD hasn't been granted by user. Cannot read pasteboard data.`，
而同一会话的启动、文件保存、渲染验收均通过。
→ 可支撑"**未授权时应用可正常启动**"，**不能**支撑"用户点粘贴时的可见表现"。

## 5. 6 项最终建议表

| 权限 | 建议 | 交给谁 |
|---|---|---|
| `READ_PASTEBOARD` | 保留；把 `reason` 串换成可读中文 | 组长 |
| `FILE_ACCESS_PERSIST` | 保留 | 组长 |
| `STORE_PERSISTENT_DATA` | 保留（暂定），RC0 做删除对照实验 | 组长 |
| `PREPARE_APP_TERMINATE` | 保留（暂定），确认 Qt 运行时是否使用 | 组长 |
| `GET_FILE_ICON` | 建议移除 | 组长（RC0 打包验证） |
| `PRINT` | 建议移除（PrintSupport 仍链接，需确认模板不使用） | 组长（RC0 打包验证） |

## 6. 未查证项（明确不编造）

| # | 未查证 | 原因 | 交给谁 |
|---|---|---|---|
| 1 | 6 项各自的 `system_grant`/`user_grant` 分级 | 检索接口余额不足；本机无 HarmonyOS 权限文档；`web_fetch` 华为文档页返回空内容（JS 渲染） | 组长 |
| 2 | `reason` 串 `qt_permission_reason_read_pasteboard` 的实际文案 | 该 string 资源在 DevEco 工程，不在本仓库 | 组长 |
| 3 | `PREPARE_APP_TERMINATE` 的 Qt 运行时依赖 | Qt HarmonyOS 源码不在本机（模板目录也未找到） | 组长 |
| 4 | 删除任意一项后包能否构建/启动 | `module.json5` 不在本仓库，本地无法打包 | 组长（RC0） |
| 5 | 真机权限拒绝/取消路径 | 无真机 | 组长 |
| 6 | 正式签名包清单是否与 2026-11-02 一致 | 只有一次 D7 产物 | 组长（RC0） |

## 7. 本次改动范围

| 文件 | 改动 |
|---|---|
| `docs/release/权限说明.md` | 按 6 项重写（含 §0 证据溯源、§2 逐项、§3 Qt 注入、§4 差异建议、§5 未验证项） |
| `docs/release/隐私政策.md` | §4 改为逐项 6 项表；§2 网络权限交叉引用指向新 §6；状态行更新 |
| `docs/release/第三方组件清单.md` | Qt 模块行拆成"鸿蒙实际链接 / 历史 qmake 路径"两行，补 Network 未链接与 PrintSupport 链接的证据 |
| `docs/evidence/member-c/2026-11-05/permissions-audit.md` | 本文件 |

**未改动 `common/` 下任何生产代码**（`git status` 可复核）。
