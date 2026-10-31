# PR #35 最新提交 b2d1163a 真机复验 R4

后续最新结果见 [R5](PR35_THEME_DEVICE_VERIFICATION_R5.md)：3c2d08dc 的四项 System 路径及回切 Dark 检查通过。本页保留 b2d1163a 的历史结果。

日期：2026-10-31，Asia/Shanghai。提交 `b2d1163ae37caf1c1ba926693bd345a0a1f64895`，四路径执行完毕及收尾时核对远端仍相同。关联 [PR #35](https://github.com/xiaotaop/leocad-harmony/pull/35)、[#33](https://github.com/xiaotaop/leocad-harmony/issues/33)。

**部分修复，尚不能作为 #33 完整修复批准。** 菜单和重启后的颜色名均已恢复；No、Yes 切换后不重启，Light Grey 颜色名仍近白难辨，两条路径额外等待 5 秒均未恢复。认可修复进展，不再沿用旧版“菜单及重启均失败”的判断。

本次请求最初检出的 1fa1adee 及实际结果见 [R3](PR35_THEME_DEVICE_VERIFICATION_R3.md)。整理时发现 C 又推送 b2d1163a（新增三组颜色诊断和测试），因此追加本轮，未把旧包结果直接套用给新提交。

## 包、设备及过程

在跟踪文件干净的独立检出切至 b2d1163a，复用本轮全新创建的 build/harmony-pr35-r3 目录增量构建；日志确认 common/lc_application.cpp 重编译、libleocad.so 重新链接，Hvigor BUILD SUCCESSFUL，重新生成调试签名。签名包 `leocad-b2d1163a-signed.hap`，80,085,379 字节，SHA-256 `38c196b601e4ac6eae42313ee61ec0d0a21807cbf357256ca2b2ed92b7106288`，hdc install -r 成功。

HAD-W32 / ARM64 / OpenHarmony 6.1.0.115 / API 23 / 3120×2080 / Qt 6.12.0，Release，safe_render=false、test_resource_paths=false。各进程渲染日志 passed=true、9 programs、0 skipped。固定模型为 tests/fixtures/render-baseline.ldr，未修改，未录屏。

每条路径均从同一原始 Dark 配置开始，视图→偏好设置→System→重置界面颜色 No/Yes→OK；截图后再等待 5 秒截图；正常关闭、读回 ColorTheme=1，重新启动同一模型观察。两轮之间恢复配置并读回哈希核对。

| 路径 | PID | 菜单 | 当前颜色名 | 截图与完整进程日志 |
|---|---:|---|---|---|
| 原 Dark | 53822 | 可读 | 可读 | [截图](evidence/leader/2026-10-31/pr35-theme-device-r4/dark-start.jpeg)、[偏好页](evidence/leader/2026-10-31/pr35-theme-device-r4/preferences-before-no.jpeg) |
| System → No → OK | 53822 | 可读 | **极浅难辨，延时仍未恢复** | [提示](evidence/leader/2026-10-31/pr35-theme-device-r4/no-reset-prompt.jpeg)、[即时](evidence/leader/2026-10-31/pr35-theme-device-r4/no-applied.jpeg)、[延时](evidence/leader/2026-10-31/pr35-theme-device-r4/no-applied-delayed.jpeg)、[日志](evidence/leader/2026-10-31/pr35-theme-device-r4/no-run.hilog.txt) |
| No 后重启 | 53885 | 可读 | 可读 | [截图](evidence/leader/2026-10-31/pr35-theme-device-r4/no-restarted.jpeg)、[日志](evidence/leader/2026-10-31/pr35-theme-device-r4/no-restart.hilog.txt) |
| 独立 Dark → System → Yes → OK | 54333 | 可读 | **极浅难辨，延时仍未恢复** | [Dark 起点](evidence/leader/2026-10-31/pr35-theme-device-r4/dark-before-yes.jpeg)、[提示](evidence/leader/2026-10-31/pr35-theme-device-r4/yes-reset-prompt.jpeg)、[即时](evidence/leader/2026-10-31/pr35-theme-device-r4/yes-applied.jpeg)、[延时](evidence/leader/2026-10-31/pr35-theme-device-r4/yes-applied-delayed.jpeg)、[日志](evidence/leader/2026-10-31/pr35-theme-device-r4/yes-run.hilog.txt) |
| Yes 后重启 | 54460 | 可读 | 可读 | [截图](evidence/leader/2026-10-31/pr35-theme-device-r4/yes-restarted.jpeg)、[日志](evidence/leader/2026-10-31/pr35-theme-device-r4/yes-restart.hilog.txt) |
| 恢复原 Dark 后启动 | 54880 | 可读 | 可读 | [截图](evidence/leader/2026-10-31/pr35-theme-device-r4/restored-dark.jpeg)、[日志](evidence/leader/2026-10-31/pr35-theme-device-r4/restored-dark.hilog.txt) |

5 个进程均 native main returned 0。最新源码 Windows Qt Release/offscreen 测试 **13 passed / 0 failed / 0 skipped**，见 [主机结果](evidence/leader/2026-10-31/pr35-theme-device-r4/host-qt-results.txt)。未声称全控件、全功能或上架验收通过。

## 新增诊断及交接

No 保存 19:10:23.886、Yes 保存 19:11:28.443 的应用 palette 为：Window #ffffff、WindowText/Text #182431、ButtonText #1e1e1e、Button #f2f2f2、Base #e4e4e4；两次重启相同。surface role 1、16 的对齐日志均实际触发。

紧随保存的 menubar 和标为 colorbutton 的采样均 enabled=true、has_own_palette=false、own_style=false、has_stylesheet=false、style/app_style 为 QOhosStyle，但仍是旧深色解析值：Window #313437、Button #2d3033、Base #232629。新增 `active_wt_bt` 和 `inactive_wt_bt` 均为 `#f0f0f0/#e0e0f4`，`disabled_wt_bt` 为 `#808080/#808080`。这证明最新三组颜色诊断已执行，也说明立即采样时应用与控件 palette 不同。

立即采样不代表事件处理后持续状态；延时截图才是颜色名残留的可见证据。重启时 UpdateStyle 执行阶段主窗口尚未创建，没有 widget 记录，不能用缺失证明控件正常。C 下一步聚焦**实时切换后颜色名控件配色传播/绘制更新**，直接定位 mColorButton（或唯一 objectName）并补事件处理后的采样。当前“第一个文本非空且有 icon 的 QToolButton”诊断选择器没有记录 text/objectName，不宜仅凭标签认定目标身份。尚未证明更具体的最终根因。

## 恢复与边界

最后 Dark 截图后正常退出，再次恢复原配置并读回：5,420 字节，SHA-256 `5575996fecc88dc99bb3513e4d8d5a68da11521b04685dfc3c6c65fa66a13631`，与本轮备份及中间恢复一致。模型读回 SHA-256 `25a650cddc8a6da11ae8edac3f71328294308244ea1280eaa9a78fbd88a1769b`，与原文件一致。完整用户配置仅留忽略的 build 目录。

设备保留 b2d1163a 新调试包，应用已退出；未改业务代码、合并 PR 或关闭 #33。#32 仍按原决定为已接受的非阻断裁切缺陷，不属于本轮残留。机器记录见 [result.json](evidence/leader/2026-10-31/pr35-theme-device-r4/result.json)，证据哈希见 [receipt.json](evidence/leader/2026-10-31/pr35-theme-device-r4/receipt.json)。
