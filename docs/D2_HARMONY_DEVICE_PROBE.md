# D2：Qt Widgets + QOpenGLWidget 真机探针

本探针用于验证 Qt 6.12 for HarmonyOS 的窗口、OpenGL ES 上下文、LeoCAD 着色器、FBO、共享上下文和高 DPI 行为。它是独立诊断 HAP，不是 LeoCAD 首发包。

2026-10-27：已完成两轮真机验证，详见 [D2 验收记录](D2_DEVICE_ACCEPTANCE.md)。

## 一键构建

```powershell
$env:DEVECO_SDK_HOME = 'C:\path\to\command-line-tools\sdk'
$env:QT_ROOT = 'D:\qt\6.12.0\harmonyos_arm64_v8a'
pwsh -File scripts/run-harmony-gl-probe.ps1
```

脚本先运行工具链门禁，再生成：

```text
build/harmony-render-probe/harmonyos-build/entry/build/default/outputs/default/entry-default-unsigned.hap
```

## 自动签名并在真机启动

前置条件：

- `devecocli auth status` 显示已登录华为开发者账号；
- 账号已完成开发者实名认证，或加入了可申请调试签名的团队；
- 真机开启开发者模式和 USB 调试，`devecocli device list` 能看到设备；
- 首次使用账号时，在华为开发者中心「ID、证书和Profile → 设备」登记当前真机 UDID，类型选择 `PC/2in1`；
- AppGallery Connect 允许为 `io.github.xiaotaop.leocad.glprobe` 生成调试签名。

若实名认证刚完成但 CLI 仍返回未实名，执行 `devecocli auth logout`，再执行
`devecocli auth login --no-prompt` 并在浏览器中重新授权，以刷新账号状态。

执行：

```powershell
pwsh -File scripts/run-harmony-gl-probe.ps1 `
  -GenerateSignature -Run -Device '<设备序列号>'
```

签名材料只写入 `build/` 生成目录，不应提交证书、Profile、密码、设备 UDID 或签名后的 HAP。

## 真机通过判据

1. 安装和 `QAbility` 启动成功，出现 “LeoCAD GL capability probe” 窗口。
2. 窗口显示蓝色背景和绿色三角形，并从 640×480 调整到约 800×500。
3. `hilog` 中的 `LEOCAD_GL_PROBE` JSON 满足 `passed=true`。
4. `context.is_gles=true`，并记录 `vendor`、`renderer`、`gl_version` 和 `glsl_version`。
5. 九组 `leocad_shaders[].linked` 全部为 `true`。
6. FBO 读回、共享纹理、高 DPI 尺寸、三角形中心像素和窗口 resize 均通过。
7. 第二次启动结果一致，再保存脱敏日志、JSON、PNG、源码提交和 HAP SHA-256。

应用在 HarmonyOS 下默认把 `render-probe.json` 和 `render-probe.png` 写到应用可写数据目录，并在完成第二帧后保留窗口 3 秒再退出。

安装后可自动采集两轮原始 JSON、PNG 与退出证据：

```powershell
pwsh -File scripts/collect-harmony-gl-probe.ps1 -Device '<设备序列号>' -Rounds 2
```

默认输出到 `build/harmony-render-probe/evidence`。脚本核对每轮新增进程日志和沙箱 JSON 一致，并确认该进程原生 main 返回 0。取回应用沙箱文件使用 `hdc file recv -b <bundleName>`；普通 shell 无权遍历该沙箱。

## 常见阻断

| 现象 | 含义 | 处理 |
| --- | --- | --- |
| `no signature file` | 安装了 unsigned HAP | 使用 DevEco CLI/Studio 自动签名 |
| `9568257 fail to verify pkcs7 file` | 商用设备不信任 OpenHarmony 公共测试证书 | 不使用本地自签；改用华为开发者调试签名 |
| `Users without real-name verification are not supported` | 当前账号不能申请华为调试签名 | 完成开发者实名认证，或登录有权限的已实名/团队账号 |
| `No teams found` | 当前账号没有可借用的团队签名权限 | 由团队管理员邀请账号，或使用自己的已实名主体 |
| `Unable to create the profile file due to missing devices` | 账号尚未登记调试设备；CLI 1.3.5/1.3.9 即使能列出本地真机，也可能在首次登记前被预检拦截 | 在开发者中心登记当前真机 UDID 后重试 |
