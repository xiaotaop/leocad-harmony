# D6 设备证据

完整步骤、结论及限制见 [组长验收报告](../../../../D6_DEVICE_ACCEPTANCE.md)。

源码 `5ade519591bf6931b5631593d34455882c10f490`；新签名 HAP 的 SHA-256 为 `6448cf5f3118ae26b860525b621863cfd61800dd3aa16848f7c33c197f53f68a`。

- `integrated-run.hilog.txt`：PID20566，面板、插入、移动、撤销/重做，正常退出。
- `input-restart.hilog.txt`：PID23172，重启、导航、失焦、窗口恢复，正常退出。
- `reports.json`：本轮实际元数据、坐标、恢复哈希及未完成项。
- `receipt.json`：逐文件原始字节 SHA-256，不包含自身。

截图来自实体设备的 `snapshot_display`，没有编辑图像或录屏。HDC UITest/uinput 注入输入，未作物理键鼠组合验证。部分截图处于选择高亮状态，实体真实颜色应参照 `green-insert-full.jpeg`；其中绿色1×1是第二轮颜色确认件，第一轮移动用的是 `3003.dat`。

准备过程、坐标错误、未确认切换应用的 Alt+Tab、无效组合键尝试和带系统网络通知的截图保留于本机被忽略的 `build/d6-device-exploration/`，未将其记为产品通过或失败证据。

核心闭环通过不等于 A 的九步交接全部通过，也不等于保存重开或上架验收通过。未完成项见报告 §4。
