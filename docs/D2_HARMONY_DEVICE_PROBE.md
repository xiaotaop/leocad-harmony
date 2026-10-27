# D2：Qt Widgets + QOpenGLWidget 真机探针

本探针用于验证 Qt 6.12 for HarmonyOS 的窗口、OpenGL ES 上下文、LeoCAD 着色器、FBO、共享上下文和高 DPI 行为。它是独立诊断 HAP，不是 LeoCAD 首发包。

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
- AppGallery Connect 允许为 `io.github.xiaotaop.leocad.glprobe` 生成调试签名。

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

## 常见阻断

| 现象 | 含义 | 处理 |
| --- | --- | --- |
| `no signature file` | 安装了 unsigned HAP | 使用 DevEco CLI/Studio 自动签名 |
| `9568257 fail to verify pkcs7 file` | 商用设备不信任 OpenHarmony 公共测试证书 | 不使用本地自签；改用华为开发者调试签名 |
| `Users without real-name verification are not supported` | 当前账号不能申请华为调试签名 | 完成开发者实名认证，或登录有权限的已实名/团队账号 |
| `No teams found` | 当前账号没有可借用的团队签名权限 | 由团队管理员邀请账号，或使用自己的已实名主体 |
