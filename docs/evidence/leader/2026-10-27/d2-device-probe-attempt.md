# D2 真机探针提前执行记录（2026-10-27）

后续状态：当日完成开发者实名认证并重新登录授权、登记真机后，已取得华为调试签名并完成两轮真机验收，见 [D2 验收记录](../../../D2_DEVICE_ACCEPTANCE.md)。下文保留首次尝试的历史阻断信息。

## 已完成

- Qt 6.12 HarmonyOS ARM64 工具链门禁通过。
- `Qt6::Widgets`、`Qt6::OpenGL`、`Qt6::OpenGLWidgets` 最小 HAP 构建成功。
- 最近一次未签名 HAP SHA-256：`128fd0c4445e6cc205b61396a0774c9a9748cc1755881afb7893a36e8dc82359`（HAP 含构建元数据，后续重建须重新记录哈希）。
- 探针在 HarmonyOS 下默认请求 OpenGL ES 3.0，并使用应用数据目录保存 JSON/PNG。
- 设备通过 USB 被 HDC 和 DevEco CLI 正确识别。
- 未签名 HAP 由 SDK `hap-sign-tool` 本地签名后通过离线 `verify-app`；该实验签名仅位于忽略的 `build/` 目录。
- DevEco CLI 官方自动签名路径已验证到账号权限检查阶段。

## 环境基线（脱敏）

| 项目 | 实测值 |
| --- | --- |
| 设备 | HUAWEI MateBook Pro / HAD-W32 / 2in1 |
| 设备序列号 | 不入库（本机 HDC 目标可见） |
| CPU ABI | arm64-v8a |
| 系统 | OpenHarmony 6.1.0.115；设备软件 6.1.0.117 |
| 设备 API | 23 Release |
| 目标/编译 SDK | 26.0.0（本机 SDK 26.0.0.105） |
| Qt | 6.12.0 for HarmonyOS arm64-v8a |
| HDC | 3.2.0f |
| bundleName | `io.github.xiaotaop.leocad.glprobe` |
| 源码 | 本记录所在提交，分支 `feature/leader-platform` |

GPU vendor/renderer/GL/GLSL 必须由应用内 `glGetString` 取得。设备 shell 对 `/proc`、DRM sysfs 和 PCI 信息设有权限限制，因此在 HAP 成功启动前不猜测这些字段。

## 当前唯一阻断

商用真机拒绝 OpenHarmony 公共测试证书：

```text
code:9568257 error: fail to verify pkcs7 file
```

本机 DevEco CLI 已登录，但账号自动签名返回：

```text
Users without real-name verification are not supported.
Complete real-name verification in AppGallery Connect.
```

`devecocli auth team list` 同时返回 `No teams found for the current user.`。因此本次没有伪造“安装成功”“窗口出现”或 GPU 结果，D2 的统一出口仍待可信调试签名解锁。

## 解锁后执行

登录已完成开发者实名认证且具有签名权限的华为账号，然后运行：

```powershell
pwsh -File scripts/run-harmony-gl-probe.ps1 `
  -GenerateSignature -Run -Device '<设备序列号>'
```

随后按 `docs/D2_HARMONY_DEVICE_PROBE.md` 保存两轮 JSON、PNG 和过滤后的 `hilog`，再将本记录更新为通过或真实失败。
