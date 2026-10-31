# 成员 A D5 真机交接：#27 颜色与相机（2026-10-31）

给组长 21:00 批次，20:30 前提交。源码为本 PR 经 B 审核的完整提交，实际构建前将完整 SHA 和签名 HAP SHA256 记入回执；产物由组长 Windows/DevEco 构建。本机只有宿主 Release 可执行文件，未生成/冒充新的 Harmony 包。

## 固定步骤（9步）

1. 用 `scripts/build-harmony-app.ps1 -GenerateSignature` 构建，明确 `LEOCAD_SAFE_RENDER=OFF`；记录完整提交、Qt/SDK、签名 HAP 哈希。测试使用同一精简库和原样 `tests/fixtures/render-baseline.ldr`。
2. 冷启动并加载固定模型，记录 PID。保留原始 `LEOCAD_GL_CONTEXT`、`LEOCAD_GL_RENDERER`、`LEOCAD_GL_FRAMEBUFFER`、`LEOCAD_GL_READBACK` 和 `LEOCAD_RESOURCE_PATHS`。读回日志通常首次绘制缩略图就出现，必须从进程启动开始采集。
3. 进入 Brick 分类选择 Red，截图同时包含名称/色块、缩略图及主视图。预期红色缩略图，既有红蓝模型颜色正确。
4. 插入所选零件，确认实体与 Red 名称/色块/缩略图同色；撤销插入，确认文档回到测试基线。
5. 对 Blue、LightBlue 重复选择和插入/撤销，分别留截图；预期蓝色保持蓝、浅蓝不偏黄。
6. 将窗口最大化、恢复；每个尺寸重复切换 Red/Blue/LightBlue，防止缓存/DPR/尺寸变化后错色。记录尺寸、DPR 和对应 FBO/读回日志，确认已有模型也未错色或被网格覆盖。
7. 切前视、顶视、默认视角并执行 ZoomExtents/适应窗口；宽窗和窄窗下模型完整可见，无额外轮廓效果仍可用。记录实际命令与截图。
8. 正常退出并再次启动同一正常包，复验颜色和相机；记录新 PID、native main 返回值。若另测基础包则明确 ON、shader6/3，完成后恢复 OFF，不能以基础包替代正常包验证。
9. 回传完整源码/包哈希、原始日志和各步截图、选择/插入一致性、窗口/相机结果及退出码。A 对照 #27 验收后再关闭，不沿用 D4 旧包结论。

## 日志解释及失败时保留

`LEOCAD_GL_READBACK` 的 `destination_format=5` 即 ARGB32；源可能为 5/6/17/18 等，以目标 Kit 实测为准，不能预填“必为 RGBA8888”。`source_stride` 为该次图像真实 bytesPerLine。只说明格式转换路径，颜色仍以截图和插入结果判断。

若仍错色，保留具体颜色码、名称/色块/缩略图/插入实体、源 format/depth/stride、DPR 与提交/包哈希；保持 #27 打开。回退会恢复旧读回缺陷；基础模式不保证绕过该问题。本次不代表系统选择器、全部材质、休眠恢复或发布验收完成。
