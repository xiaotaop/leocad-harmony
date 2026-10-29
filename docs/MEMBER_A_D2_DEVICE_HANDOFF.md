# 成员 A D2：主程序上下文补丁真机交接

批次：2026-10-28 21:00；未赶上则进入下一次组长批次。
模块负责人：成员 A；构建、安装及证据回传：组长。

2026-10-28 组长补测：已用包含本补丁的完整主程序完成上下文/Surface/GLES/共享检查、
固定两砖模型显示、窗口变化和正常退出重启。生产包与日志见
[D3 集成验收](D3_DEVICE_ACCEPTANCE.md)。网格覆盖模型的绘制问题仍交 A 处理，
不能将这些结果理解为完整渲染质量或生命周期压力测试通过。以下保留原交接要求。

## 已知基线

组长 PR #7 的 `42d00d83` 归档了 10/27 的两轮独立 GL 探针：
MateBook Pro / HAD-W32 / API23 / Maleoon 916 / GLES 3.2 / GLSL ES 3.20 / DPR 1.9。
A 已核对两轮 JSON、PNG 的 SHA-256、全部九组着色器、PID 日志和采集收据。
这些结果属于组长的既有探针与签名包，不是本次生产补丁的真机结果。

本次 PR 基于 main `32ce8425`；没有叠加或修改 PR #7 的探针、签名和宿主脚本。
产物为 `lc_context`/`lc_viewwidget` 补丁、共享上下文测试、宿主日志及对照图。
A 本机无 HarmonyOS Qt Kit/真机，未生成或声称运行新的 HAP。
具体补丁 SHA 以本次 A PR 的 head 为准。

## 需要采集的新增日志

`LEOCAD_GL_CONTEXT` 后跟一行 JSON：

- `stage=renderer_offscreen`：`gles_build`、`is_gles`、`context_format`、
  `surface_requested_format`、`surface_format`、`sharing`、GPU/GLSL、`passed`、`error`。
- `stage=view_initialize`：主视图实际格式、共享组、DPR、窗口尺寸。

错误关键字：`offscreen_setup_requires_gui_thread`、`share_context_invalid`、
`offscreen_context_create_failed`、`offscreen_context_not_sharing`、
`offscreen_surface_create_failed`、`offscreen_make_current_failed`、
`renderer_context_api_mismatch`、`renderer_requires_gles3`。

## 8 步验收

1. 在本次 A 提交上使用组长已验证的 Qt/SDK/Hvigor 构建完整 LeoCAD 调试 HAP，
   记录源码 SHA、包 SHA-256、Qt/SDK、设备和系统；若主窗口平台入口尚未就绪，记录阻塞。
2. 安装并启动，采集启动至退出的原始日志和窗口截图。
3. 核对 `renderer_offscreen` 的 `passed=true`、`error` 为空、`is_current=true`，
   且 `surface_requested_format` 与 `context_format` 完全一致。
4. 核对 `gles_build=true`、`is_gles=true`、实际 GLES 主版本至少 3；
   不得通过删除错误日志或强制关闭检查来绕过 API 不一致。
5. 核对 `share_requested=true`、`sharing=true`，以及视图初始化日志的共享关系。
6. 显示固定 `tests/fixtures/render-baseline.ldr`；观察两块红/蓝积木，无黑屏或明显缺件。
   若完整资源/模型路径尚未接入，记录该步骤待 D3/D5，不把最小三角形代替模型。
7. 调整窗口大小，退出后重新启动；记录重复启动是否成功及上下文错误。
8. 回传全部日志、JSON 行、截图、源码/包哈希和进程退出状态；A 负责分析新问题。

## 回退与限制

按 PR 撤回 `common/lc_context.cpp`、`common/lc_viewwidget.cpp` 及新增 helper 即恢复
原上下文流程；诊断测试和文档可保留。API mismatch 表示 Kit/编译路径需要纠正，
不能以在 GLES 上禁用 shaders 的方式规避。

该补丁只开始处理 D2/D3 上下文兼容。模型拾取、输入、窗口重建后的资源恢复、
首帧性能与长期稳定性仍按后续日程验收。
