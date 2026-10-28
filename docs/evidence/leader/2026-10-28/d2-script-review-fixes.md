# D2 宿主脚本审查修正（2026-10-28）

本轮只修改 Windows 宿主构建/采集脚本及说明文档。根据组长要求不进行新一轮真机验证，保留 [2026-10-27 的两轮真机通过结论](../../../D2_DEVICE_ACCEPTANCE.md)。探针 C++、着色器及 CMake 配置未改动。

## 三项修正

1. 采集由固定等待 5 秒改为轮询新报告和同一 PID 的原生 main 退出日志，默认每轮等待 30 秒，可用 `-TimeoutSeconds` 调整。旧报告、并发报告、格式错误、非零退出和超时均不能产生通过结果。
2. `-SdkRoot` 同时决定 Qt/CMake 与 DevEco CLI 的 SDK、CLT 和 Hvigor 路径；暂时清除优先级更高的旧 Studio 配置，并在成功或异常后恢复所有原有进程环境变量。
3. 文档明确证书、私钥及 Profile 位于用户的 `.ohos/config/`；生成工程在 `build/` 内保存配置引用和 HAP。

## 本机验证

执行 `pwsh -File scripts/Test-HarmonyProbeTools.ps1`，10 项离线模拟全部通过：

- 新报告在 5.2 秒后才出现，仍能成功采集。
- 报告先出现，等待匹配的退出日志后才通过。
- 仅有旧报告时超时。
- 非零退出被拒绝。
- 其他 PID 的退出日志不能通过。
- 多个新报告被拒绝。
- 错误报告头被拒绝。
- SDK/CLT/Hvigor 选择同步，旧 Studio 配置被屏蔽，真实子进程继承选定环境；正常返回后恢复旧环境。
- 模拟构建异常后恢复旧环境。
- 原本未设置的环境变量在结束后仍未设置。

该测试只向轮询函数提供内存中的日志回调，未执行 HDC、签名或真机命令。历史两轮 JSON/PNG 哈希及原签名 HAP SHA-256 已重新核对，一致；历史证据文件未修改。

另在独立目录 `build/harmony-d2-script-check` 完整运行修订后的构建脚本，显式传入 SDK/Qt 路径，未启用 `-GenerateSignature` 或 `-Run`。Qt/CMake、C++、ArkTS 与 HAP 打包全部成功，进程退出码 0；Hvigor 明确跳过签名。

- SDK 26.0.0.105 / Qt 6.12.0 / Hvigor 6.26.8。
- 新未签名 HAP SHA-256：`fadab13669b48d9dd04c8d5da6a1dffb90c9fdc248985f55cd287b43b1d68a83`。
- 该包仅用于检查宿主构建流程，未安装或运行，不替代历史真机验证的签名包。
- PowerShell 语法检查及 `git diff --check` 通过。
