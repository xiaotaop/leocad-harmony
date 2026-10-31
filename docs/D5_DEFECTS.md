# D5 真机缺陷与交接

日期：2026-10-31。版本、包哈希、设备和实际步骤见 [D5 真机报告](D5_DEVICE_ACCEPTANCE.md)。组长复现后决定将[前视裁切 #32](https://github.com/xiaotaop/leocad-harmony/issues/32)作为已接受的非阻断已知缺陷，关闭该issue，本期不要求修复；关闭不代表代码已修复。[System主题 #33](https://github.com/xiaotaop/leocad-harmony/issues/33)继续开放，由成员C（Leaf-21）跟进。原始证据保留，其他旧issue未自动关闭。

| 编号 / 级别 | 现象与证据 | 负责人及下一步 | 状态 |
|---|---|---|---|
| D4-01 / #27 | Red、Blue、Light Blue 的名称/色块/缩略图/插入实体在最大化、恢复窗口下一致，重启后三色仍正确；实际读回源format18、目标5 | A核对本轮截图和原始日志后确认原颜色问题收口 | **当前版本真机复验通过，待A确认关闭**；不等同相机全部通过 |
| D4-02 / #14 | 偏好常规页移除更新、库路径、POV-Ray、LGEO；渲染页移除Blender；保留颜色/人仔设置与必要渲染项 | C核对裁剪证据；原残留可以按已修复记录，主题问题单独跟进 | **裁剪真机复验通过**，没有自动关闭远端issue |
| D5-01 / P2·非阻断，[#32](https://github.com/xiaotaop/leocad-harmony/issues/32) | 两砖基线切Front，执行“视图→缩放至全屏”（ZoomExtents），红砖左侧、蓝砖右侧超出视图；两种窗口及重启后均复现。[最大化](evidence/leader/2026-10-31/d5-device/restart-front-fit-maximized.jpeg)、[恢复窗口](evidence/leader/2026-10-31/d5-device/restart-front-fit-restored.jpeg) | 组长复现后接受为非阻断的易用性缺陷，本期不要求A修复；可切Home查看完整模型，未来优化可复用本轮证据 | **已接受并关闭issue，未修复**；不阻断D5出口或本次验收PR合入 |
| D5-02 / P2，[#33](https://github.com/xiaotaop/leocad-harmony/issues/33) | 从深色改System，重置颜色选择No，点OK后主菜单文字和颜色名不可见；新PID重启仍复现。[保存后](evidence/leader/2026-10-31/d5-device/system-theme-applied.jpeg)、[重启后](evidence/leader/2026-10-31/d5-device/restart-system.jpeg) | C主责、组长配合Qt平台定位；检查Harmony默认style/palette/菜单绘制，分别覆盖重置颜色Yes/No。修复前保留深色作为已验证路径；不能仅凭ColorTheme写盘宣称主题可用 | **待修**；原因及是否既有平台问题尚未证明。本轮已恢复用户原深色配置 |
| D5-OBS-01 / 观察项 | 弹出/关闭窗口时Qt OHOS出现xComponent无receiver、Ace回收context为空日志；本轮可继续操作且native main最终0 | 组长后续平台稳定性回归关注；若关联空白窗口/崩溃，再补最小复现和级别 | 原始日志保留，未证明造成可见故障，未计作第三个已确认功能缺陷 |

已有 #13（异常库/模态退出）、#15（D7–D8系统文件选择和外部读写）、#16（发布权限）、#28（D7文案/布局）不由本轮结果自动收口。D4报告作为历史证据保留，不把旧截图改写为本轮结果。

2026-10-31 补充：[PR #35 真机复验](PR35_THEME_DEVICE_VERIFICATION.md)已覆盖 No/Yes 保存及重启，四项仍复现 #33。日志确认默认样式为非空 `ohos`，没有进入新增 fallback；#33 继续待修，暂不建议将该版本作为修复合并。用户原深色配置已完整恢复。

**固定模型稳定显示的D5出口通过；前视适应窗口作为非阻断已知缺陷接受，System主题仍需修复和复验。** 组长已完成今天的验证、采样、效果取舍和缺陷记录，没有宣称全组所有质量项或上架验收完成。
