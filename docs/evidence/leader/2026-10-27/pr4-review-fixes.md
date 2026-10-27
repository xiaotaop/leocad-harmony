# PR #4 审核意见修复验证

日期：2026-10-27

验证提交基线：`819c9b995a2380ad73fda520e81e5a6040d35eb2` 加本次待提交修改。

## Windows 根 CMake 链接

- 环境：Windows、Qt 6.12.0 MinGW 64-bit、MinGW 13.1.0、Ninja、zlib 1.2.13。
- 根 `CMakeLists.txt` 在 `WIN32` 下显式链接 `wininet`。
- `cmake --build build/windows-review --parallel 4` 完成 96 个步骤，成功生成并链接 `leocad.exe`。
- `leocad.exe --version` 输出 `LeoCAD Version 26.09`，退出码为 0。

## HarmonyOS 离线包权限

- HarmonyOS 目标不再查找或链接 `Qt6::Network`，并通过 `LC_DISABLE_NETWORK` 使用离线 HTTP 桩；在线套装导入入口在该目标上隐藏。
- `leocad_make_hap` 成功，Hvigor 共执行 33 个任务并报告 `BUILD SUCCESSFUL`。
- HAP：`build/harmony/harmonyos-build/entry/build/default/outputs/default/entry-default-unsigned.hap`
- 大小：76,982,078 字节。
- SHA-256：`687F5AA17921A2E9D5AC22567EFDB4BAE67A029F9A2BDD9D9E8A94ED44B62374`。
- 直接读取 HAP 内的 `module.json`，确认 `ohos.permission.INTERNET` 与 `ohos.permission.GET_NETWORK_INFO` 均不存在。
- 当前 `requestPermissions` 仅包含：
  - `ohos.permission.STORE_PERSISTENT_DATA`
  - `ohos.permission.FILE_ACCESS_PERSIST`
  - `ohos.permission.PREPARE_APP_TERMINATE`
  - `ohos.permission.READ_PASTEBOARD`
  - `ohos.permission.GET_FILE_ICON`
  - `ohos.permission.PRINT`

## Qt Kit 误判回归

- `check-toolchain.ps1 -QtRoot <HarmonyOS Qt 6.12 ARM64> -RequireQt`：通过。
- 将桌面 MinGW Qt 6.12 作为 `-QtRoot`：失败，明确报告缺少 HarmonyOS `.so`、OHOS 目标标识和 `arm64-v8a` 工具链标识。
- `QT_HOST_PATH` 只用于单独显示宿主工具状态，不再参与 HarmonyOS target Qt 判定；通用 `CMAKE_PREFIX_PATH` 也不再作为目标 Kit 候选。
