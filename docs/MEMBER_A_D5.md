# 成员 A D5：缩略图颜色与模型视图回归（2026-10-31）

基线 main `9210a20d93c7bdfb689c85d29e4c78d6e6acd3cb`（组长 #26 已合并）。D5 原任务是模型显示、相机、适应窗口及无额外效果时可用；新增优先缺陷为 [#27](https://github.com/xiaotaop/leocad-harmony/issues/27)。

## 修正与原因

`BeginRenderToImage()` 分配 ARGB32，原 `OnDraw()` 却直接复制 FBO `toImage()` 的字节。Qt 的读回格式由后端决定，不能假设与目标相同。新增共用 `lcCopyRenderImageTile()`，在复制前 `convertToFormat(ARGB32)`，再以源/目标 `constScanLine()/scanLine()` 读取实际 stride。分块投影、上下翻转及目标位置计算保持原逻辑。缩略图最后一次转换本来就存在，但不能修复此前已经被标错格式的字节。

负对照直接执行旧 memcpy：RGBA8888 的 `(201,26,9)` 变成 `(9,26,201)`；Blue `(0,85,191)` 变成 `(191,85,0)`；浅蓝 `(180,210,227)` 变成 `(227,210,180)`。相同输入经过生产 helper 后恢复原色。没有添加颜色控件特判或末端红蓝交换。

宿主实际 FBO 单采样/4x MSAA 均返回 ARGB32_Premultiplied（6），转换目标为 ARGB32（5）；透明度也必须按格式语义转换。Harmony 的实际返回值还需日志确认。主程序新增 `LEOCAD_GL_READBACK`，在首次出现一种读回格式时记录源 format/depth/stride/width/height 与目标 format。该日志不等同于颜色正确或真机验收通过。

## 已执行验证

- 独立读回测试 12/12：五种格式、奇数宽度、带 padding 的行跨度、非原点矩形、目标哨兵边界、旧路径错色负例、透明/半透明、真实 GL FBO 单采样与 MSAA；CI 同时运行 Mesa desktop GL 和 GLES3。
- 实际生产模型套件 64/64（原 59 + 本次 5）：真实 `lcThumbnailManager → lcView::OnDraw → FBO → QPixmap`，Red/Blue/LightBlue，尺寸 36/52/36、DPR1/2 及缓存复用；默认/前/顶视角、宽窗/窄窗/恢复尺寸、ZoomExtents 后八个包围盒顶点位于视锥内、画面中心和实体面积正确；4091×133 与 133×4091 触发实际分块且在拼接处保持颜色。
- 基础模式独立进程 7/7（5 行 + init/cleanup），实际 renderer 日志为 6 shader、跳过 3；正常模式为 9/0。禁用边线/条件线时仍有正确实体颜色和完整相机适应结果。Windows CI 也执行该子集。
- Qt6.12 Release 主程序完整构建成功；使用官方 `LEOCAD_TEST_RESOURCE_PATHS=ON` 运行启动 CTest 1/1，涵盖 help/version 正常退出与无效参数失败。

新增图形集成用例只进入宿主模型套件，Harmony 模型测试仍为原 50 项；Windows 额外含一个文件锁用例，因此完整套件应为 65 项。Mac 测试使用 canonical `/private/tmp` 避免旧有 `/var` 别名路径断言。没有修改 B 的路径断言。

构建命令、源码 SHA256、原始测试输出、shader/readback 日志、各颜色缩略图及相机截图见 [证据目录](evidence/member-a/2026-10-31/build-record.json)。`receipt.json` 校验本目录文件。截图来自测试驱动的真实生产视图，并非真机截图。

## 待设备复验与交付边界

A 的代码修正和宿主验证已完成，交 B 审核、组长合并；[#27](https://github.com/xiaotaop/leocad-harmony/issues/27) 保持打开，待组长采集新提交正常包的读回格式并确认名称/色块、缩略图及插入实体一致后再关闭。已有 D4 证据证明旧主视图和相机可用，不能替代本修复包的真机复验。最大化/恢复、高分屏原生窗口、整套材质与长期稳定性也不能由本次离屏用例替代。

今晚批次请求见 [D5 真机交接](MEMBER_A_D5_DEVICE_HANDOFF.md)。回退可撤销本 PR，但会恢复 #27；`LEOCAD_SAFE_RENDER` 只降级额外效果，不能替代颜色修正。
