# 成员 A：D1 图形与输入风险清单

检查基线：`3e2e15b5bcd6ba87720bd2dfebffd49fd48ee3c0`（2026-10-27 main）。
以下是源码检查发现的风险及后续验证项，不代表鸿蒙真机已经复现。
今天不改业务渲染路径，先保留桌面对照和独立探针。

| ID / 优先级 | 源码依据与风险 | 验证方式 / 下一步 | 依赖与降级边界 |
|---|---|---|---|
| GL-01 / P0 候选 | `lc_context.cpp::CreateOffscreenContext` 没有用实际 context format 设置 Surface；不同 EGL 配置可能导致 `makeCurrent` 失败 | 真机记录实际格式、Surface 有效性和切换结果；D2/D3 补匹配格式、失败日志 | A 与组长；关闭缩略图不能绕过当前启动时必需的全局离屏上下文 |
| GL-02 / P0 候选 | 同函数设置 global share context 后只检查 create/isValid，没有确认共享组和共享对象 | 探针 `areSharing` 与共享纹理可见性；主程序再验证全局上下文到主视图/缩略图的共享链 | A；必要时评估 UI 线程同步上传，不能凭创建成功假定共享成功 |
| GL-03 / P0 候选 | `lc_global.h` 与 `CreateShaderPrograms` 通过编译宏选择 GLSL 110 或 300 ES，实际上下文 API/版本未对齐验证 | 对照 `is_gles`、GLSL 和 Qt 编译宏；目标 GLES 必须满足当前 ES 3.0 着色器要求 | A 与组长锁 Kit；GLES 下不能将“关闭 shaders”当作可用固定功能降级 |
| GL-04 / P0 候选 | shader 编译状态只在 debug 检查；链接失败后 Program=0，但未输出已收集的 InfoLog，也未让 InitializeRenderer 失败 | 探针九组材质逐一记录编译/链接日志；后续把主程序失败传递到启动错误提示 | A；不能继续使用 Program 0 并把黑屏当成功启动 |
| GL-05 / P1 候选 | `lc_viewwidget.cpp::initializeGL` 只重新设置 context；`lcContext` 的 program ID 和部分资源全局静态 | 真机窗口重建、最小化/恢复、休眠恢复后检查上下文和资源；确认共享组生命周期后决定重建策略 | A 与组长生命周期回调；避免共享资源被某一视图重复销毁 |
| GL-06 / P1 候选 | `lc_view.cpp::BeginRenderToImage`、缩略图路径和 widget 默认 FBO 切换需要目标平台核对 | 检查 FBO 附件/有效性、图像读回、切回 widget 的正确 FBO；探针显式绑定 `defaultFramebufferObject()` | A 与 C；先关缩略图/非核心图像导出，但必须保留主视图正确显示 |
| GL-07 / P1 候选 | `lc_viewwidget.cpp` 用 DPR 将逻辑坐标转整数像素，Qt6 鼠标部分路径仍用整数 x/y；屏幕切换没有独立处理 | 在 1、1.25、1.5、2 倍缩放下检查角点及中心拾取，调整窗口尺寸和显示器 | A；探针只证明图像尺寸，不等同于已通过模型拾取测试 |
| GL-08 / P1 候选 | `wheelEvent` 先计算 accumulator/8，再 Steps/15，但每次减掉 Steps*8；小增量可能触发 0 步并丢失余量 | 输入 15 次 angleDelta=8，应累计为一个 120 单位滚轮刻度；后续补行为测试和保留余量修复 | A；本日是静态分析，尚未修改/实测原版输入行为 |
| GL-09 / P1 候选 | GL 扩展标志全局保存；如 anisotropic 在新 context 不支持时没有明确重置 | 上下文重建后重新检测，检查是否残留旧 GPU/旧 context 的能力和上限 | A；关各向异性等额外效果，不影响基础实体显示 |
| GL-10 / P1 候选 | 共享上下文不等于允许任意线程调用；本机 `supportsThreadedOpenGL=false` | 记录资源上传线程与 current context；目标机另测，所有 Surface 创建留在 UI 线程 | A 与 C 的库加载边界；优先 UI 线程上传，不以本机结果推断鸿蒙能力 |

## D2/D3 执行顺序

1. 组长提供目标 Qt/SDK/系统版本及最小 HAP 宿主；A 接入探针并取得原始日志。
2. 先处理 GL-01/02/03：上下文、Surface、共享链和 GLSL 路线。
3. 再处理 GL-04：失败必须可诊断，避免 Release 包静默黑屏。
4. 输出最小兼容补丁，桌面回归后送当晚真机批次。
5. 10/31 前保证固定模型显示；输入与 DPI 行为在 11/3 前完成。

## 接口边界

- 组长维护入口、生命周期及根构建；A 的建议走独立 PR，不同时大改。
- A 向 B 约定拾取结果、选择/变换命令；B 维护模型容器和历史。
- C 维护库发现与索引；A 维护图形上传。涉及 `lc_library.*` 的 PR 互审。
- UI 线程拥有主 GL context；探针没有授权后台线程绕过 Qt 的线程约束。

依据：[Qt QOpenGLWidget](https://doc.qt.io/qt-6/qopenglwidget.html)、
[Qt QOpenGLContext](https://doc.qt.io/qt-6/qopenglcontext.html)、
[Qt QOffscreenSurface](https://doc.qt.io/qt-6/qoffscreensurface.html)。
