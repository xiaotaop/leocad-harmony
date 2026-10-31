# PR #35 System 主题真机复验

后续提交 `a439bf20` 的第二轮复验见 [R2 报告](PR35_THEME_DEVICE_VERIFICATION_R2.md)，No/Yes 保存及重启仍失败。本文件保留 `ba36110b` 的历史结果。

日期：2026-10-31，Asia/Shanghai。执行：组长。关联 [PR #35](https://github.com/xiaotaop/leocad-harmony/pull/35)、[缺陷 #33](https://github.com/xiaotaop/leocad-harmony/issues/33)。

**结论：未通过。No、Yes 两条路径保存后和重启后，主菜单文字、当前颜色名均仍不可见。暂不建议将此版本作为 #33 的修复合并，#33 应继续开放。** 先前代码复审认可了防御分支及测试改进，但该结论不能替代本轮真机结果。不能以构建成功、测试通过、ColorTheme 写盘或正常退出认定问题已修复。

## 1. 被测版本及环境

| 项目 | 实际值 |
|---|---|
| 源码 | PR #35 `ba36110b078bfe4e4ab95a4922f0048629c06575`；复验结束核对远端仍为此 SHA |
| 构建 | Release / HarmonyOS ARM64，Qt 6.12.0；`scripts/build-harmony-app.ps1 -BuildDirectory build/harmony-pr35 -GenerateSignature`；Hvigor BUILD SUCCESSFUL |
| 模式 | 正常渲染，safe_render=false；未启用 test_resource_paths |
| 签名 HAP | `leocad-ba36110b-signed.hap`，80,068,486 字节 |
| HAP SHA-256 | `88039ecaa31608e78722b7c65b6e157198800da34924fdf0c679fd9c57f45716` |
| 安装 | `hdc install -r` 成功；保留应用数据 |
| 设备 | HAD-W32，ARM64，OpenHarmony 6.1.0.115，API 23，3120×2080；不记录设备唯一标识 |
| 渲染 | Maleoon 916，OpenGL ES 3.2；各轮 LEOCAD_GL_RENDERER passed=true，9 个 program，0 skipped |
| 固定模型 | 仓库 `tests/fixtures/render-baseline.ldr`，267 字节，SHA-256 `25a650cddc8a6da11ae8edac3f71328294308244ea1280eaa9a78fbd88a1769b` |

Windows/offscreen 测试是代码层证据：最新源码测试 6 passed / 0 failed；移除实际 fallback setStyle、只保留回填的变异版本 5 passed / 1 failed，说明新增断言能发现该变异。本轮真机并未触发这个 fallback 分支。

## 2. 实际步骤与结果

测试前应用已退出，备份完整 LeoCAD.conf。先安装上述包，在原深色配置下打开固定模型。经“视图 → 偏好设置”将主题从 Dark 改为 System，在“是否重置界面颜色”提示中选择指定选项，再点偏好设置 OK。观察主窗口，正常关闭后读取 ColorTheme，然后重新启动同一模型观察。No 和 Yes 之间恢复同一份测试前深色配置，避免前一轮重置颜色影响后一轮。

| 路径 | PID | 实际观察 | 结果与证据 |
|---|---:|---|---|
| 原深色启动 | 32914 | 主菜单“文件/编辑/视图/零件/子模型/帮助”和当前颜色名 Light Grey 可见 | 通过：[深色基线](evidence/leader/2026-10-31/pr35-theme-device/dark-start.jpeg)、[偏好设置](evidence/leader/2026-10-31/pr35-theme-device/preferences-before-no.jpeg) |
| System → No → OK | 32914 | 菜单栏文字、当前颜色名不可见；保存 ColorTheme=1 | **失败**：[No 提示](evidence/leader/2026-10-31/pr35-theme-device/no-reset-prompt.jpeg)、[保存后](evidence/leader/2026-10-31/pr35-theme-device/no-applied.jpeg)、[完整进程日志](evidence/leader/2026-10-31/pr35-theme-device/no-run.hilog.txt) |
| No 路径后重新启动 | 33405 | ColorTheme=1，文字仍不可见 | **失败**：[重启截图](evidence/leader/2026-10-31/pr35-theme-device/no-restarted.jpeg)、[完整进程日志](evidence/leader/2026-10-31/pr35-theme-device/no-restart.hilog.txt) |
| 独立恢复深色 → System → Yes → OK | 33699 | 重置颜色后模型背景变白；菜单栏文字、当前颜色名仍不可见；保存 ColorTheme=1 | **失败**：[独立深色起点](evidence/leader/2026-10-31/pr35-theme-device/dark-before-yes.jpeg)、[Yes 提示](evidence/leader/2026-10-31/pr35-theme-device/yes-reset-prompt.jpeg)、[保存后](evidence/leader/2026-10-31/pr35-theme-device/yes-applied.jpeg)、[完整进程日志](evidence/leader/2026-10-31/pr35-theme-device/yes-run.hilog.txt) |
| Yes 路径后重新启动 | 34084 | ColorTheme=1，文字仍不可见 | **失败**：[重启截图](evidence/leader/2026-10-31/pr35-theme-device/yes-restarted.jpeg)、[完整进程日志](evidence/leader/2026-10-31/pr35-theme-device/yes-restart.hilog.txt) |
| 最后恢复原深色配置后启动 | 34596 | 菜单文字和 Light Grey 重新可见 | 通过：[恢复截图](evidence/leader/2026-10-31/pr35-theme-device/restored-dark.jpeg)、[完整进程日志](evidence/leader/2026-10-31/pr35-theme-device/restored-dark.hilog.txt) |

上述 5 个进程均正常关闭，日志含 native main `returned 0`。这只证明本轮没有以崩溃结束，不代表 System 主题可用。未录屏。

## 3. 日志确认的范围

No 保存时（17:34:44.241 / 17:34:44.275）与 Yes 保存时（17:36:06.211 / 17:36:06.244）均为：

```json
{"default":"ohos","active_before":"","class_before":"QStyleSheetStyle"}
{"active_after":"ohos","class_after":"QOhosStyle","window":"#ffffff"}
```

两次重启均为：

```json
{"default":"ohos","active_before":"ohos","class_before":"QOhosStyle"}
{"active_after":"ohos","class_after":"QOhosStyle","window":"#ffffff"}
```

因此，**本设备捕获到的默认样式名非空，为 `ohos`**；保存主题前空的 active_before 属于 QStyleSheetStyle 包装层，不能据此宣称默认样式名为空。实际恢复到 QOhosStyle，没有出现新增 fallback 的日志或 Could not restore 警告，说明本轮未进入回退分支。当前补丁覆盖的“默认样式名为空/恢复失败”情形不能解释或修复本轮可见故障。

C 下一步应围绕实际 `ohos` / QOhosStyle 路径，检查去除深色 stylesheet 后主菜单及颜色名控件的文字角色、palette 和原生绘制，再针对 No/Yes 保存及重启复验。本轮未采集这些控件全部前景色角色，也未证明 Qt 平台、palette 或应用控件中哪一层是最终根因；不要把新假设写成已确认原因。

## 4. 收尾及边界

测试前配置 5,420 字节，SHA-256 `5575996fecc88dc99bb3513e4d8d5a68da11521b04685dfc3c6c65fa66a13631`。最后恢复深色并截图、正常退出后，再次恢复原备份并读回：字节数、SHA-256 完全一致。完整个人配置仅存本地忽略的 build 目录。固定模型读回 SHA-256 与原文件一致。设备保留本轮 PR #35 调试包，应用已退出，未回滚安装包。

本轮没有改动业务代码、合并 PR 或关闭 #33。前视裁切 #32 仍按组长决定作为已接受的非阻断缺陷关闭，截图中该现象不影响本轮主题结论。未把本轮复验扩大为 FPS、全部偏好设置、文件选择或上架验收通过。

机器可读结果见 [result.json](evidence/leader/2026-10-31/pr35-theme-device/result.json)，证据哈希见 [receipt.json](evidence/leader/2026-10-31/pr35-theme-device/receipt.json)。原 D5 证据保持原样。
