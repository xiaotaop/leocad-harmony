# Shared GL context regression

D2 的独立 Qt Test 目标，直接使用生产渲染器调用的 `common/lc_glcontext.h`。
不修改组长 PR #7 中的 `tests/render/` 真机探针。

```sh
cmake -S tests/render-context -B build/render-context \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.12.0/macos -DCMAKE_BUILD_TYPE=Release
cmake --build build/render-context --parallel 4
build/render-context/leocad-render-context-tests \
  -o build/render-context/results.txt,txt \
  -o build/render-context/results.xml,junitxml
```

需要 Qt 6.5+ Gui/Test 和有 GL 能力的图形会话。macOS D2 实测 Qt 6.12.0。
Linux CI 使用 Xvfb + Mesa 软件 GL；Qt 的 `offscreen` 平台插件不能代替真实 GL 后端。

覆盖 11 个用例，另有 init/cleanup 两项，Qt Test 合计 13 项：

- 六组桌面/GLES API 与最低 GLES 版本判定，保留桌面兼容路径。
- 共享纹理的四通道像素读回；故意预设错误格式后，验证子上下文继承父上下文，
  Surface 请求格式与新上下文的实际格式一致，screen 和共享组正确。
- 无效 share context、已创建对象和非 GUI 线程调用在原生操作前被拒绝。
- 对非 current context 只记录元数据；只有 current context 才读取 GPU/GL 信息。

纹理测试用 `glFinish` 同步父上下文上传，验证共享对象和内容，
不把跨上下文命令调度或异步上传性能混入这个测试。
测试没有模拟原生驱动 create/makeCurrent 失败，相关失败分支仍须随目标设备日志核对。

当前独立能力探针的设备支持结果不能代替这些生产改动的真机验收。
接入批次见 [D2 交接](../../docs/MEMBER_A_D2_DEVICE_HANDOFF.md)。
