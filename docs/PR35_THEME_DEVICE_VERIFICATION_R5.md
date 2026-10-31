# PR #35 主题修复真机复验 R5

日期：2026-10-31，Asia/Shanghai。关联 [PR #35](https://github.com/xiaotaop/leocad-harmony/pull/35)、[#33](https://github.com/xiaotaop/leocad-harmony/issues/33)。

后续状态：PR #35 已合入 main（297341e3），#33 已按 completed 关闭。以下保留实际被测源码、包及复验完成时的记录；没有把合并提交当作重新构建验证。

**本轮主题复验通过，先前阻断意见已满足，可以合并该修复。** Dark → System 的 No、Yes 两条路径，切换后及重启后，菜单和 Light Grey 当前颜色名全部可读；额外覆盖 System 启动后改回 Dark，也可读。整理时曾误判回切深色菜单为深字深底，重新逐张对照原始截图后已撤回；原图实际为浅字深底，没有证据支持该失败判断。

## 被测版本及包

实际构建源码为 `3c2d08dc1138cec21d8e5a83e195fd195481923c`，在干净的独立检出中增量构建，编译 lc_application.cpp、重新链接 libleocad.so，Hvigor BUILD SUCCESSFUL，生成新调试签名，hdc install -r 成功。收尾时远端为 `ce2012bc0fdfd84d66213ac17f5de4b761c17668`；两者 diff 仅 docs/D5_偏好设置裁剪.md 新增 31 行，功能代码及测试相同，未把包标为 ce2012bc 构建。

签名包 `leocad-3c2d08dc-signed.hap`：80,094,152 字节，SHA-256 `9121cb3500aba55488a369ff05289c7fe04740b3d7db71364135b7a29788e6ab`。包及完整构建日志仅留忽略的 build 目录。

设备 HAD-W32 / ARM64 / OpenHarmony 6.1.0.115 / API 23 / 3120×2080 / Qt 6.12.0。Release，safe_render=false、test_resource_paths=false。固定模型 tests/fixtures/render-baseline.ldr，未录屏。

## 实际路径与结果

No、Yes 各自从同一份原始 Dark 配置开始，视图 → 偏好设置 → System → 重置界面颜色选 No/Yes → OK；截图后额外等待 5 秒，再正常退出、读回配置、重启同一模型。两轮之间恢复原配置并核对哈希。

| 路径 | PID | 菜单 / 当前颜色名 | 证据 |
|---|---:|---|---|
| 原 Dark | 59659 | 均可读 | [起点](evidence/leader/2026-10-31/pr35-theme-device-r5/dark-start.jpeg)、[偏好页](evidence/leader/2026-10-31/pr35-theme-device-r5/preferences-before-no.jpeg) |
| System → No → OK | 59659 | 均可读，等待 5 秒仍正常 | [提示](evidence/leader/2026-10-31/pr35-theme-device-r5/no-reset-prompt.jpeg)、[即时](evidence/leader/2026-10-31/pr35-theme-device-r5/no-applied.jpeg)、[延时](evidence/leader/2026-10-31/pr35-theme-device-r5/no-applied-delayed.jpeg)、[完整日志](evidence/leader/2026-10-31/pr35-theme-device-r5/no-run.hilog.txt) |
| No 后重启 | 60447 | 均可读；展开视图菜单文字可读 | [重启](evidence/leader/2026-10-31/pr35-theme-device-r5/no-restarted.jpeg)、[菜单](evidence/leader/2026-10-31/pr35-theme-device-r5/system-view-menu.jpeg) |
| 上述 System 启动进程中改 Dark → No → OK | 60447 | 均可读，等待 3 秒仍正常 | [提示](evidence/leader/2026-10-31/pr35-theme-device-r5/return-dark-reset-prompt.jpeg)、[即时](evidence/leader/2026-10-31/pr35-theme-device-r5/return-dark-applied.jpeg)、[延时](evidence/leader/2026-10-31/pr35-theme-device-r5/return-dark-delayed.jpeg)、[该进程完整日志](evidence/leader/2026-10-31/pr35-theme-device-r5/no-restart-and-return-dark.hilog.txt) |
| 独立 Dark → System → Yes → OK | 61417 | 均可读，等待 5 秒仍正常 | [独立起点](evidence/leader/2026-10-31/pr35-theme-device-r5/dark-before-yes.jpeg)、[提示](evidence/leader/2026-10-31/pr35-theme-device-r5/yes-reset-prompt.jpeg)、[即时](evidence/leader/2026-10-31/pr35-theme-device-r5/yes-applied.jpeg)、[延时](evidence/leader/2026-10-31/pr35-theme-device-r5/yes-applied-delayed.jpeg)、[完整日志](evidence/leader/2026-10-31/pr35-theme-device-r5/yes-run.hilog.txt) |
| Yes 后重启 | 61623 | 均可读 | [截图](evidence/leader/2026-10-31/pr35-theme-device-r5/yes-restarted.jpeg)、[完整日志](evidence/leader/2026-10-31/pr35-theme-device-r5/yes-restart.hilog.txt) |
| 恢复原配置后启动 Dark | 62181 | 均可读 | [截图](evidence/leader/2026-10-31/pr35-theme-device-r5/restored-dark.jpeg)、[完整日志](evidence/leader/2026-10-31/pr35-theme-device-r5/restored-dark.hilog.txt) |

四个 System 阶段退出后配置均为 ColorTheme=1；回切 Dark 后为 0。No 后重启的日志同时包含其后的 Dark 切换，不将整个进程日志都描述成 System 阶段。没有覆盖不重启、单进程连续 Dark → System → Dark，也不声称全控件或所有主题组合通过。

## 诊断、主机测试及边界

No、Yes 应用 palette 均为 Window #ffffff、WindowText/Text #182431、ButtonText #1e1e1e、Button #f2f2f2、Base #e4e4e4。两次实时切换的 menubar / 标为 colorbutton 的采样，在即时和约 1 秒后的 LEOCAD_THEME_DELAYED 阶段均与上述角色一致，has_own_palette=true；配合延时原始截图，确认 R4 的颜色名残留本轮已消失。诊断选择器仍非唯一 mColorButton 标识，不能仅凭标签证明目标身份。

5 个进程均有 renderer passed=true、9 programs、0 skipped、safe_render=false，并正常 native main returned 0。最新源码 Windows Qt Release/offscreen 测试 **14 passed / 0 failed / 0 skipped**，见 [主机结果](evidence/leader/2026-10-31/pr35-theme-device-r5/host-qt-results.txt)。

另保留 [System 偏好页](evidence/leader/2026-10-31/pr35-theme-device-r5/system-preferences.jpeg) 供外观核对：未改设置时 OK 显示很浅；本轮实际改变主题后可保存。没有充分证据把该外观认定为新功能缺陷，不据此新增阻断意见。未证明本轮以外的平台日志问题根因。

## 收尾

最后 Dark 截图后正常退出，再次恢复原配置并读回核对：5,420 字节，SHA-256 `5575996fecc88dc99bb3513e4d8d5a68da11521b04685dfc3c6c65fa66a13631`，与备份及中间恢复完全一致。模型读回 SHA-256 `25a650cddc8a6da11ae8edac3f71328294308244ea1280eaa9a78fbd88a1769b`，与原文件一致。完整配置不提交。

设备保留本轮新调试包，应用已退出。未修改业务代码、合并 PR 或关闭 issue。#33 原复现路径已通过，建议随修复合入收口；#32 仍按用户决定为已接受的非阻断缺陷。机器记录见 [result.json](evidence/leader/2026-10-31/pr35-theme-device-r5/result.json)，原始证据哈希见 [receipt.json](evidence/leader/2026-10-31/pr35-theme-device-r5/receipt.json)。
