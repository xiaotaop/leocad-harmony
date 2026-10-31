# D5 真机缺陷与交接

日期：2026-10-31。版本、包哈希、设备和实际步骤见 [D5 真机报告](D5_DEVICE_ACCEPTANCE.md)。当前状态：[前视裁切 #32](https://github.com/xiaotaop/leocad-harmony/issues/32)由组长接受为非阻断已知缺陷并关闭，本期不要求修复；[System 主题 #33](https://github.com/xiaotaop/leocad-harmony/issues/33)经 [R5 真机复验](PR35_THEME_DEVICE_VERIFICATION_R5.md)通过，修复 PR #35 已合入，issue 已按 completed 关闭。两项关闭含义不同，原始证据保留；其他旧 issue 不由本次结果自动关闭。

| 编号 / 级别 | 现象与证据 | 负责人及下一步 | 状态 |
|---|---|---|---|
| D4-01 / #27 | Red、Blue、Light Blue 的名称/色块/缩略图/插入实体在最大化、恢复窗口下一致，重启后三色仍正确；实际读回源format18、目标5 | A核对本轮截图和原始日志后确认原颜色问题收口 | **当前版本真机复验通过，待A确认关闭**；不等同相机全部通过 |
| D4-02 / #14 | 偏好常规页移除更新、库路径、POV-Ray、LGEO；渲染页移除Blender；保留颜色/人仔设置与必要渲染项 | C核对裁剪证据；原残留可以按已修复记录，主题问题单独跟进 | **裁剪真机复验通过**，没有自动关闭远端issue |
| D5-01 / P2·非阻断，[#32](https://github.com/xiaotaop/leocad-harmony/issues/32) | 两砖基线切Front，执行“视图→缩放至全屏”（ZoomExtents），红砖左侧、蓝砖右侧超出视图；两种窗口及重启后均复现。[最大化](evidence/leader/2026-10-31/d5-device/restart-front-fit-maximized.jpeg)、[恢复窗口](evidence/leader/2026-10-31/d5-device/restart-front-fit-restored.jpeg) | 组长复现后接受为非阻断的易用性缺陷，本期不要求A修复；可切Home查看完整模型，未来优化可复用本轮证据 | **已接受并关闭issue，未修复**；不阻断D5出口或本次验收PR合入 |
| D5-02 / P2，[#33](https://github.com/xiaotaop/leocad-harmony/issues/33) | 首轮从深色改 System，No 保存及重启后菜单和颜色名不可见；[历史保存后](evidence/leader/2026-10-31/d5-device/system-theme-applied.jpeg)、[历史重启后](evidence/leader/2026-10-31/d5-device/restart-system.jpeg)。最新 [R5](PR35_THEME_DEVICE_VERIFICATION_R5.md) 的 No/Yes 保存及重启四路径均可读 | C 已提交修复，组长完成逐轮真机复验并批准 PR #35；用户原配置已恢复 | **已修复并复验通过，PR #35 已合入；issue 已按 completed 关闭** |
| D5-OBS-01 / 观察项 | 弹出/关闭窗口时Qt OHOS出现xComponent无receiver、Ace回收context为空日志；本轮可继续操作且native main最终0 | 组长后续平台稳定性回归关注；若关联空白窗口/崩溃，再补最小复现和级别 | 原始日志保留，未证明造成可见故障，未计作第三个已确认功能缺陷 |

已有 #13（异常库/模态退出）、#15（D7–D8系统文件选择和外部读写）、#16（发布权限）、#28（D7文案/布局）不由本轮结果自动收口。D4报告作为历史证据保留，不把旧截图改写为本轮结果。

以下按时间保留各轮当时的结果，当前状态以上表及末尾更新为准。

2026-10-31 补充：[PR #35 真机复验](PR35_THEME_DEVICE_VERIFICATION.md)已覆盖 No/Yes 保存及重启，四项仍复现 #33。日志确认默认样式为非空 `ohos`，没有进入新增 fallback；#33 继续待修，暂不建议将该版本作为修复合并。用户原深色配置已完整恢复。

同日第二轮：[最新提交 `a439bf20` 复验](PR35_THEME_DEVICE_VERIFICATION_R2.md)已重新构建、签名、安装。主机 10 项测试通过；真机四项仍失败，应用级 palette 日志已有可读颜色配对，但实际菜单及颜色名仍不可见，需继续检查控件级样式和原生绘制。原始配置再次完整恢复，#33 状态不变。

同日第三轮：[提交 `1fa1adee` 复验](PR35_THEME_DEVICE_VERIFICATION_R3.md)确认**部分修复**：No/Yes 切换后主菜单正常，两条路径重启后菜单及颜色名均正常；切换当下颜色名仍近白难辨，Yes 延时 5 秒仍复现。主机 12 项测试通过。#33 残留已收窄到不重启的颜色名更新，不能继续沿用“菜单及重启均失败”的旧结论，也未认定完整修复。原配置已恢复。

随后新增 `b2d1163a` 的三组颜色诊断，[R4 复验](PR35_THEME_DEVICE_VERIFICATION_R4.md)已再次构建安装并覆盖全部四路径；结论仍为部分修复，No/Yes 均等待 5 秒仍有颜色名残留，重启后恢复。13 项主机测试通过，完整原配置再次恢复。

**固定模型稳定显示的D5出口通过；前视适应窗口作为非阻断已知缺陷接受，System主题仍需修复和复验。** 组长已完成今天的验证、采样、效果取舍和缺陷记录，没有宣称全组所有质量项或上架验收完成。

同日第五轮更新：[R5 真机复验](PR35_THEME_DEVICE_VERIFICATION_R5.md)实际重新构建安装 `3c2d08dc`，远端后续 `ce2012bc` 仅改文档，功能代码相同。**#33 原 No/Yes 切换后及重启后的四路径均通过，菜单及当前颜色名可读；额外 System 启动后切回 Dark 也正常，可以合并该修复。** 14 项主机测试通过，原配置完整恢复。整理中曾误判回切 Dark 菜单为异常，重新对照原始截图后撤回，不计为缺陷。以上为最新结论，前文表格及 R1–R4 保留历史复现状态；#33 尚未自动关闭，建议随修复合入收口。

PR #34 更新时再次核对远端：**PR #35 已合入 main（297341e3），#33 已按 completed 关闭**。验收分支已同步该 main，前文 R5 的“建议合入收口”为当时状态，现已完成。
