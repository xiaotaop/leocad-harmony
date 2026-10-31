# PR #35 最新主题修复真机复验 R2

更新：`1fa1adee` 的 [R3 复验](PR35_THEME_DEVICE_VERIFICATION_R3.md)已确认菜单及重启后的颜色名恢复，仅切换当下的颜色名仍难辨；以下保留 `a439bf20` 历史结果。

日期：2026-10-31（Asia/Shanghai）。组长执行。关联 [PR #35](https://github.com/xiaotaop/leocad-harmony/pull/35)、[缺陷 #33](https://github.com/xiaotaop/leocad-harmony/issues/33)。

**未通过，当前版本仍不建议作为 #33 的修复合并。** Windows/offscreen 的 10 项测试通过，但真机 No/Yes 保存和重启四项仍显示主菜单文字、当前颜色名不可见或近乎不可辨。深色恢复后文字正常。没有合并 PR 或关闭 issue。

## 被测包与操作

源码为 `a439bf20eb7e9375426a3e363f11a6b69d730eb1`，复验结束远端 HEAD 仍相同。相较上一轮，包含 `416c3973` 的调色板对比度修复及 `a439bf20` 的 stylesheet teardown 顺序调整。使用干净跟踪状态的独立检出，构建目录 `build/harmony-pr35-r2`，完整重新编译后签名。构建日志确认重新编译 common/lc_application.cpp、链接 libleocad.so，Hvigor BUILD SUCCESSFUL。

签名包 `leocad-a439bf20-signed.hap`：80,080,203 字节，SHA-256 `192f9bee3ef6e367ead540ec2eeacfc247e2f43d16a8f5242e39341f1eeaecee`。已通过 `hdc install -r` 安装成功，非沿用上轮包。

设备 HAD-W32 / ARM64 / OpenHarmony 6.1.0.115 / API 23 / 3120×2080，Qt 6.12.0。Release，LEOCAD_SAFE_RENDER=OFF、LEOCAD_TEST_RESOURCE_PATHS=OFF。各进程正常渲染诊断均为 passed=true、9 programs、0 skipped。固定模型仍为 tests/fixtures/render-baseline.ldr；前视裁切 #32 保持已接受的非阻断已知缺陷，不纳入主题判断。未录屏。

每轮从同一份测试前 Dark 配置打开固定模型，经“视图 → 偏好设置”切换 System，分别选择重置颜色 No/Yes，点 OK 后观察；正常关闭、读取 ColorTheme，再启动新进程观察。No 与 Yes 两轮之间完整恢复原始配置并读回校验 SHA-256，避免串用设置。

## 实测结果

| 路径 | PID | 结果 | 截图 / 完整进程日志 |
|---|---:|---|---|
| Dark 基线及偏好页 | 40324 | 菜单与 Light Grey 可读 | [主窗口](evidence/leader/2026-10-31/pr35-theme-device-r2/dark-start.jpeg)、[偏好页](evidence/leader/2026-10-31/pr35-theme-device-r2/preferences-before-no.jpeg) |
| System → No → OK | 40324 | **失败**，菜单空白、Light Grey 极浅；ColorTheme=1 | [提示](evidence/leader/2026-10-31/pr35-theme-device-r2/no-reset-prompt.jpeg)、[保存后](evidence/leader/2026-10-31/pr35-theme-device-r2/no-applied.jpeg)、[日志](evidence/leader/2026-10-31/pr35-theme-device-r2/no-run.hilog.txt) |
| No 后重启 | 40999 | **失败**，菜单及颜色名仍不可见；ColorTheme=1 | [截图](evidence/leader/2026-10-31/pr35-theme-device-r2/no-restarted.jpeg)、[日志](evidence/leader/2026-10-31/pr35-theme-device-r2/no-restart.hilog.txt) |
| 独立恢复 Dark → System → Yes → OK | 41157 | **失败**，背景重置变白，菜单及颜色名仍不可见；ColorTheme=1 | [Dark 起点](evidence/leader/2026-10-31/pr35-theme-device-r2/dark-before-yes.jpeg)、[提示](evidence/leader/2026-10-31/pr35-theme-device-r2/yes-reset-prompt.jpeg)、[保存后](evidence/leader/2026-10-31/pr35-theme-device-r2/yes-applied.jpeg)、[日志](evidence/leader/2026-10-31/pr35-theme-device-r2/yes-run.hilog.txt) |
| Yes 后重启 | 41446 | **失败**，菜单及颜色名仍不可见；ColorTheme=1 | [截图](evidence/leader/2026-10-31/pr35-theme-device-r2/yes-restarted.jpeg)、[日志](evidence/leader/2026-10-31/pr35-theme-device-r2/yes-restart.hilog.txt) |
| 恢复原 Dark 配置后启动 | 41617 | 菜单及 Light Grey 恢复可读 | [截图](evidence/leader/2026-10-31/pr35-theme-device-r2/restored-dark.jpeg)、[日志](evidence/leader/2026-10-31/pr35-theme-device-r2/restored-dark.hilog.txt) |

以上 5 个 PID 均以 native main returned 0 正常退出。Windows Qt 6.12.0 Release/offscreen 最新源码测试为 **10 passed / 0 failed / 0 skipped**，见 [主机原始结果](evidence/leader/2026-10-31/pr35-theme-device-r2/host-qt-results.txt)。主机测试检查的 palette/style 不能代替原生设备实际像素效果。

## 新证据及下一步

No 保存（18:19:07.568）、No 重启（18:19:26.987）、Yes 保存（18:20:13.869）、Yes 重启（18:20:25.417）的新增诊断均含：

```json
{"active_after":"ohos","class_after":"QOhosStyle","window":"#ffffff","windowtext":"#182431","text":"#182431","buttontext":"#ffffff","button":"#000000","base":"#e4e4e4"}
```

新增 windowtext/text/buttontext/button/base 字段证明本轮运行到了最新诊断代码。应用级 QPalette 的上述配对已经有明显对比度，未出现 Palette role 修复警告；默认样式名仍是非空 ohos，也未进入恢复失败的 fallback。然而截图中主菜单及颜色名仍不可见。因此，本轮不能再把“应用级标准 palette 的文字颜色对比度不足”认定为已确认根因，也不能仅以修复该配对或主机测试通过宣告 #33 修复。

C 应继续采集实际 QMenuBar、颜色名 QToolButton 等控件的 palette（含颜色组）、stylesheet、style 与 enabled 状态，并检查显式控件样式、去除应用 stylesheet 后的继承/重新 polish、QOhosStyle 最终使用的绘制角色。以上仅为排查方向，本轮未证明最终根因或实现新修复。

## 恢复与证据校验

原配置及最终退出后再次恢复并读回的配置均为 5,420 字节，SHA-256 `5575996fecc88dc99bb3513e4d8d5a68da11521b04685dfc3c6c65fa66a13631`。中间恢复也核对相同哈希。完整配置只留本地忽略的 build 目录。模型读回 267 字节，SHA-256 `25a650cddc8a6da11ae8edac3f71328294308244ea1280eaa9a78fbd88a1769b`，与原模型一致。设备保留本轮 a439bf20 调试包、应用已退出，未回滚安装包。

机器可读记录见 [result.json](evidence/leader/2026-10-31/pr35-theme-device-r2/result.json)，逐文件 SHA-256 见 [receipt.json](evidence/leader/2026-10-31/pr35-theme-device-r2/receipt.json)。[上轮结果](PR35_THEME_DEVICE_VERIFICATION.md)及原始证据保留。
