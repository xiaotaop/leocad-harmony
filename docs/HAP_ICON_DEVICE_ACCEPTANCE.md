# HarmonyOS HAP 图标修复与真机验证

日期：2026-10-29。**修复通过：启动画面、主窗口标题栏和任务栏均显示 LeoCAD 红色积木图标，正常退出重启后保持。**

## 改动与原因

源码提交 `dbc6f074`（父版本 `fbcdc5e1`，已含 C 的 #17/#19）。C 的 Qt 图标资源已存在，但 Harmony HAP 使用单独的应用/Ability 元数据和启动资源，不会由 `setWindowIcon()` 自动替换。

`CMakeLists.txt` 在 OHOS 主程序目标增加：

- `QT_HARMONYOS_APP_ICON` 指向已有 `tools/icon/512x512/apps/leocad.png`，由 Qt 打包工具生成应用与 Ability 的 `$media:leocad` 引用。
- 将当前已安装 Qt 的 Harmony 模板复制到构建目录，并以同一张图替换模板的 `startIcon.png`；通过 `QT_HARMONYOS_PACKAGE_SOURCE_DIR` 交给打包工具。

使用完整的当前 Qt 模板是因为该属性指定模板来源；不在仓库维护一份可能过期的全量模板，不修改 Qt 安装目录，也不只手改会被重新生成的 build 文件。此配置覆盖直接 CMake HAP 构建和现有构建/签名脚本；独立 modeltests 包保持原配置。

## 构建和静态验证

执行 `scripts/build-harmony-app.ps1`，使用已有 Qt6.12 Harmony ARM64 Kit、SDK、官方调试签名。构建及签名成功。Qt 模板仍有原有 ArkTS NODE 弃用警告，未作为无警告构建宣称。

- 签名 HAP：80,056,246 字节。
- SHA-256：`f3b5716606bce3e9cbc34268c0a942d14a37489a6548e45adb1cc63920f602ff`。
- 固定副本：`build/harmony-d3-evidence/leocad-dbc6f074-icons-signed.hap`。
- 图标原图 SHA-256：`a784800d4e8c87bfd2c6d92956dd6494ce38fd71d81d03d738f8e33c87a86140`。
- 生成项目中的 AppScope `leocad.png`、entry `leocad.png` 和 `startIcon.png` 均与该原图逐字节一致。
- `AppScope/app.json5` 的 app.icon 与 `module.json5` 的 QAbility.icon 均为 `$media:leocad`；startWindowIcon 保持 `$media:startIcon`，对应图已替换。

## 真机验证

HUAWEI HAD-W32、ARM64、API23，覆盖安装新 HAP，不清除用户数据。

1. PID 23220：启动过渡画面显示红色积木（[截图](evidence/leader/2026-10-29/hap-icons/start.jpeg)）；主窗口标题栏和任务栏显示同一红色图标（[截图](evidence/leader/2026-10-29/hap-icons/ready.jpeg)）。
2. 点击主窗口关闭按钮，原始 hilog 中 native `libleocad.so` main 返回 0。
3. 重新启动 PID 23711，标题栏和任务栏仍显示红色图标（[截图](evidence/leader/2026-10-29/hap-icons/restart.jpeg)）；再次正常关闭，main 返回 0。

原始日志与文件哈希见 [证据目录](evidence/leader/2026-10-29/hap-icons/) 的 `receipt.json`。没有使用强停代替正常退出；未提交签名材料或完整设备标识。

本次关闭前一轮验收中的“系统默认图标”差异。菜单分组、分类显示和网格覆盖模型等项目仍按各自记录处理；不代表商店最终视觉规范已审定。
