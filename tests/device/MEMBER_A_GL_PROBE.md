# 成员 A：首批真机 GL 探针请求

目标批次：2026-10-28 21:00（若平台宿主提前具备条件，可在 10/27 提前执行）。
负责人：A 分析和修复，组长构建 HAP、安装并回传证据。

## 前置条件与产物状态

- 使用包含本请求的 A 分支提交，记录实际 SHA；基线为
  `3e2e15b5bcd6ba87720bd2dfebffd49fd48ee3c0`。
- 已有 `tests/render/main.cpp` 和资源清单，只有 macOS 构建/测试结果；**尚无真机 HAP**。
- 组长先建立 Qt Widgets + QOpenGLWidget 的最小 HarmonyOS HAP 宿主，
  将探针 main 和 `resources/shaders/*.glsl`（资源前缀 `/probe`、BASE 为 `resources`）
  接入诊断目标，链接 Widgets/OpenGL/OpenGLWidgets。
- 原生启动参数应包含 `--output <应用可写沙箱>/render-probe.json`，需要 PNG 时另传
  `--screenshot <应用可写沙箱>/render-probe.png`；目标 Kit 使用 GLES 时传 `--gles`。
  由宿主完成 argv 传递，不能默认把桌面命令行直接当 HAP 启动命令。
- 固定 Qt、SDK、系统、设备版本及 HAP 构建号；主线程创建 Surface。

## 验证步骤（7 步）

1. 安装诊断 HAP，记录提交号、包哈希、设备、系统、Qt/SDK 和启动参数。
2. 启动并采集 Qt 日志/hilog，观察是否出现蓝底绿色三角形。
3. 等待自动从 640×480 调整为约 800×500；程序正常完成后退出。
4. 提取 JSON/PNG 及 `LEOCAD_GL_PROBE` 原始日志，记录进程退出状态。
5. 核对 `context.valid`、实际 API/GLSL、9 组 `linked`、FBO 读回和共享纹理可见性。
6. 核对 `widget.size_matches_dpr`、`triangle_pixel_passed`、`resize_completed`，
   如宿主限制窗口尺寸，请记录限制并标为待分析，不能手改 JSON 为通过。
7. 再次启动，确认结果可复现；失败时保存最短复现、崩溃栈/黑屏录屏及完整日志。

## 预期结果

退出状态 0、JSON `passed=true`、九组材质均链接成功；PNG 为蓝底绿色三角形。
若窗口适配或共享资源失败，允许结果为失败，但须保留证据用于 A 的下一批补丁。
Release 主程序的模型绘制、拾取、撤销及保存不属于这个诊断程序的验收范围。

## 失败时优先尝试

1. 先核对目标 Qt Kit 的 API 和启动参数；桌面 GL 与 GLES 不一致时使用对应模式重测。
2. 保持 samples=0，避免把 MSAA 或额外效果引入最小路径。
3. 独立诊断 HAP 可直接撤回；应用不链接该探针，无需修改主应用开关。
4. 共享上下文失败时先分析 EGL/format，不把禁用 shaders 当成 GLES 的通用修复。

回传：JSON、PNG、原始日志、提交/包哈希、环境版本、退出状态。
