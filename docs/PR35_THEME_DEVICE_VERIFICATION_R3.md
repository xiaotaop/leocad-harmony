# PR #35 主题真机复验 R3：部分修复

执行后 C 又提交 `b2d1163a`，已追加构建安装和四路径真机复验，见 [R4 最新报告](PR35_THEME_DEVICE_VERIFICATION_R4.md)。本文件保留 1fa1adee 的历史结果。

2026-10-31，Asia/Shanghai。被测提交 `1fa1adeed1cb03eddc8b24b2f105e7d2669c49bf`；结束时核对 [PR #35](https://github.com/xiaotaop/leocad-harmony/pull/35)远端 HEAD 未改变。

**结论：菜单及重启后的颜色名已恢复；未重启时当前颜色名仍极浅难辨，#33 尚未完整修复。** 当前不建议作为完整修复批准合并。[R2](PR35_THEME_DEVICE_VERIFICATION_R2.md)的“四项均失败”只适用于旧提交，不能用于描述本版本。

## 版本与步骤

从跟踪文件干净的独立检出构建，全新目录 `build/harmony-pr35-r3`；日志确认重新编译 common/lc_application.cpp、链接 libleocad.so，Hvigor BUILD SUCCESSFUL。Release / HarmonyOS ARM64 / Qt 6.12.0，safe_render=false、test_resource_paths=false。签名 HAP `leocad-1fa1adee-signed.hap`，80,086,615 字节，SHA-256 `1a029c7cdf48c896b9bdca143e91930e94d8b9993e4bd0d71abc93dcea1916fd`；`hdc install -r` 成功，保留应用数据。新增 LEOCAD_THEME_WIDGET 及 Palette surface 日志确认新代码实际执行。

设备 HAD-W32 / ARM64 / OpenHarmony 6.1.0.115 / API 23 / 3120×2080。初始 HDC 显示连接但 shell 无响应，重启本机 HDC 服务后 echo、配置读回恢复；未改设备 USB 设置。备份配置时应用已退出。

No/Yes 两轮分别从同一份原始 Dark 配置打开固定模型，经“视图→偏好设置”改 System，在重置界面颜色提示选 No/Yes，点 OK 后观察；正常关闭读取 ColorTheme，再启动新 PID 观察。两轮之间完整恢复原始配置并读回核对哈希。Yes 保存后额外等待 5 秒再次截图。固定模型仍为 tests/fixtures/render-baseline.ldr，未录屏。

## 逐项结果

| 阶段 | PID | 菜单 | 当前颜色名 | 证据 |
|---|---:|---|---|---|
| 原 Dark 基线 | 51041 | 可读 | Light Grey 可读 | [截图](evidence/leader/2026-10-31/pr35-theme-device-r3/dark-start.jpeg)、[偏好页](evidence/leader/2026-10-31/pr35-theme-device-r3/preferences-before-no.jpeg) |
| System → No → OK | 51041 | **已恢复可读** | **极浅难辨，未通过** | [提示](evidence/leader/2026-10-31/pr35-theme-device-r3/no-reset-prompt.jpeg)、[保存后](evidence/leader/2026-10-31/pr35-theme-device-r3/no-applied.jpeg)、[日志](evidence/leader/2026-10-31/pr35-theme-device-r3/no-run.hilog.txt) |
| No 后重启 | 51498 | 可读；文件菜单展开可读 | **Light Grey 可读** | [截图](evidence/leader/2026-10-31/pr35-theme-device-r3/no-restarted.jpeg)、[展开菜单](evidence/leader/2026-10-31/pr35-theme-device-r3/no-restarted-file-menu.jpeg)、[日志](evidence/leader/2026-10-31/pr35-theme-device-r3/no-restart.hilog.txt) |
| 独立 Dark → System → Yes → OK | 51597 | **已恢复可读** | **极浅难辨；额外等待 5 秒仍未恢复** | [Dark 起点](evidence/leader/2026-10-31/pr35-theme-device-r3/dark-before-yes.jpeg)、[提示](evidence/leader/2026-10-31/pr35-theme-device-r3/yes-reset-prompt.jpeg)、[保存后](evidence/leader/2026-10-31/pr35-theme-device-r3/yes-applied.jpeg)、[延时截图](evidence/leader/2026-10-31/pr35-theme-device-r3/yes-applied-delayed.jpeg)、[日志](evidence/leader/2026-10-31/pr35-theme-device-r3/yes-run.hilog.txt) |
| Yes 后重启 | 51992 | 可读 | **Light Grey 可读** | [截图](evidence/leader/2026-10-31/pr35-theme-device-r3/yes-restarted.jpeg)、[日志](evidence/leader/2026-10-31/pr35-theme-device-r3/yes-restart.hilog.txt) |
| 恢复原 Dark 配置后启动 | 52344 | 可读 | 可读 | [截图](evidence/leader/2026-10-31/pr35-theme-device-r3/restored-dark.jpeg)、[日志](evidence/leader/2026-10-31/pr35-theme-device-r3/restored-dark.hilog.txt) |

四个 System 阶段均读回 ColorTheme=1。五个 PID 均 native main returned 0，渲染诊断 passed=true、9 programs、0 skipped。Windows Qt 6.12.0 Release/offscreen 最新源码测试 **12 passed / 0 failed / 0 skipped**，见 [主机结果](evidence/leader/2026-10-31/pr35-theme-device-r3/host-qt-results.txt)。本轮没有做完整上架、性能或全控件验收。

## 新增日志与剩余定位

四个 System 阶段均实际触发 surface role 1、16 从暗色改为 #f2f2f2 的警告。应用级最终值一致：

```json
{"active_after":"ohos","class_after":"QOhosStyle","window":"#ffffff","windowtext":"#182431","text":"#182431","buttontext":"#1e1e1e","button":"#f2f2f2","base":"#e4e4e4"}
```

保存 No 时 19:03:06.006、保存 Yes 时 19:04:15.104，立即采样的 menubar 和标为 colorbutton 的控件仍报告旧深色值：windowtext=#f0f0f0、buttontext=#e0e0f4、text=#e0e0e0、window=#313437、button=#2d3033、base=#232629。两者 enabled=true、has_own_palette=false、own_style=false、has_stylesheet=false，style_class/app_style 均为 QOhosStyle。

这些立即日志说明当时应用级与控件级解析结果不同；不能由这一时点断定事件处理后控件永久保持旧配色。实际延时截图确认的是颜色名可读性仍异常。重启阶段没有 widget 日志；代码仅在 UpdateStyle 内采样，而启动时主窗口尚未就绪，因此不能把日志缺失解读为控件状态正常。

建议 C 聚焦**同一进程 Dark→System 后颜色名控件的 palette/style 传播及重绘**，修复后无需重启即可读，再覆盖 No/Yes 即时与延时状态。诊断最好直接定位 mColorButton 或给它唯一 objectName，并在事件处理后延时采样；当前实现选第一个文本非空且有 icon 的 QToolButton，没有记录 text/objectName，日志中的 colorbutton 标签本身不能独立证明命中了目标。以上为排查方向，不把具体传播/绘制机制定为已证明根因。

## 收尾

最终深色截图并正常退出后，再次恢复完整原配置并读回，5,420 字节，SHA-256 `5575996fecc88dc99bb3513e4d8d5a68da11521b04685dfc3c6c65fa66a13631`，与本轮备份及中间恢复完全一致。完整配置仅保存在忽略的 build 目录。模型读回 267 字节，SHA-256 `25a650cddc8a6da11ae8edac3f71328294308244ea1280eaa9a78fbd88a1769b`，与原文件一致。

设备保留 1fa1adee 新调试包，应用已退出。未改业务代码、合并 PR 或关闭 #33；#32 继续作为已接受的非阻断已知缺陷，截图中的前视裁切不影响本轮主题判断。机器记录见 [result.json](evidence/leader/2026-10-31/pr35-theme-device-r3/result.json)，证据哈希见 [receipt.json](evidence/leader/2026-10-31/pr35-theme-device-r3/receipt.json)。
