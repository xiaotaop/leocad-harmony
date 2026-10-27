# Member A: GL capability probe

独立 Qt Widgets 诊断程序，不改变 LeoCAD 主程序、偏好设置或根构建文件。
需要 C++17、CMake 3.21+ 和 Qt 6.5+ 的 Widgets/OpenGL/OpenGLWidgets；
本次实际验证版本是 Qt 6.12.0。Windows、Linux 和 HarmonyOS 尚未实测。

## Build and run

```sh
python3 scripts/Build-MemberA.py --qt-prefix /path/to/Qt/6.12.0/macos --run
```

可用 `--cmake /path/to/cmake`、`--build-dir /path/to/build` 和
`--report-dir /path/to/results` 指定工具及输出位置。Windows 的 Qt 前缀应指向
已配置 MSVC 环境的对应 Kit；不是上面的 macOS 路径。

也可直接构建：

```sh
cmake -S tests/render -B build/render-probe \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.12.0/macos -DCMAKE_BUILD_TYPE=Release
cmake --build build/render-probe --config Release --parallel 4
build/render-probe/leocad-render-probe --output build/render-probe/result.json \
  --screenshot build/render-probe/widget.png
```

在支持 OpenGL ES 的目标 Kit 上加 `--gles`，请求 ES 3.0。若平台静默回落到
桌面 GL，`requested_api_honored=false`，退出码为 1，不把回落当作 GLES 通过。
普通模式请求桌面 OpenGL 2.1，以匹配上游 GLSL 110；默认不启用 MSAA。
必须使用有图形显示能力的平台插件；Qt 的 `offscreen` 插件不能替代真实
`QOpenGLWidget` 合成路径。没有图形会话时只构建，不加 `--run`。

## Checks and output

- Qt、OS、架构、平台插件、实际 GL/GLSL、GPU、上下文格式及功能标志。
- 上传 VBO，画绿色三角形；读取窗口中心像素，避免“编译成功但画面全黑”误报。
- 从 640×480 改为 800×500，检查 framebuffer 尺寸与实际 DPR 一致。
- 创建含深度/模板附件的 FBO，清除并读回红色像素。这里测试附件建立及颜色读回，
  不宣称已覆盖遮挡、深度比较和所有模板运算。
- 在 UI 线程创建匹配格式的共享上下文及离屏 Surface，通过 `makeCurrent`、
  `areSharing` 和 `glIsTexture` 检查共享纹理对象可见性。没有测试跨线程上传。
- 编译并链接 `resources/shaders/` 的全部九组材质着色器。前缀与 D1 基线
  `common/lc_context.cpp::CreateShaderPrograms` 对应；主程序前缀/材质表变化时须同步探针。
- 可选保存 PNG。写报告或指定 PNG 失败也返回非零。

JSON 的 `passed` 为所有必需检查的合取，成功退出码为 0；失败为 1。
程序同时向 Qt 日志输出 `LEOCAD_GL_PROBE` 加一行完整 JSON，便于真机采集。
没有完成窗口帧时，15 秒后写失败报告；平台插件在 QApplication 初始化阶段崩溃
或外部终止时可能来不及写 JSON，因此必须同时检查退出状态和原始日志。

探针使用实际上下文选择 GLSL 前缀，而主程序仍按 `LC_OPENGLES` 编译宏选择。
因此探针通过只证明设备能力，不证明主程序的编译宏、绘制路径或 HAP 集成正确。
macOS 对照结果见 [D1 记录](../../docs/MEMBER_A_D1.md)。
