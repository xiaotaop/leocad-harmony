# D5 真机证据索引

完整结论见 [D5 真机报告](../../../../D5_DEVICE_ACCEPTANCE.md)，责任安排见 [D5 缺陷表](../../../../D5_DEFECTS.md)。原始JPEG为3120×2080设备截图，没有录屏；颜色判断为目视检查，不是像素完全一致断言。

| 文件 | 含义 |
|---|---|
| `normal-main.hilog.txt` | PID10733，正式颜色/窗口/相机/偏好轮次，包含首次GL上下文、读回、资源以及正常退出0 |
| `restart-system.hilog.txt`、`restart-system.jpeg` | PID14319，主题设置跨正常退出/重启保留，但System菜单文字不可见 |
| `restart-dark.hilog.txt` | PID14905，原偏好恢复后的三色、相机复验及正常退出0 |
| `baseline-maximized.jpeg`、`restored-baseline.jpeg` | 原始两砖基线 |
| `red/blue/lightblue-inserted-maximized/restored.jpeg` | 各色单独插入后，点空白去除选择高亮以判断真实颜色；恢复窗口图可能受旧相机取景裁切，不作相机通过证据 |
| `red/blue/lightblue-undone-maximized/restored.jpeg` | 每次撤回到两砖，不累积插入，标题无修改星号 |
| `red/lightblue-selected-maximized.jpeg` | 插入前的颜色名称、色块与缩略图 |
| `front/top/home-fit-maximized/restored.jpeg` | 首轮各视角后执行ZoomExtents；前视裁切，不能全部记通过 |
| `restart-front/top/home-fit-maximized/restored.jpeg` | PID14905复验同一问题，Top/Home完整，Front左右裁切 |
| `restart-red/blue/lightblue-maximized.jpeg` | 原配置重启后三色继续正确；没有再次插入，不当作重启后的插入证据 |
| `preferences-general/rendering.jpeg` | 裁剪后的常规/渲染页，保留必要项 |
| `preferences-theme-changed.jpeg` | 切System后询问是否重置颜色的模态框；本轮选择No |
| `preferences-theme-system.jpeg` | System已选，尚未点主对话框OK；不当作保存成功的唯一证据 |
| `system-theme-applied.jpeg` | 点OK保存后主窗口样式及文字消失现象 |
| `preferences-reopened-dark.jpeg` | 恢复原偏好后再次打开成功，深色/默认语言 |
| `restart-dark.jpeg`、`restart-restored.jpeg`、`final-restored.jpeg` | 原配置重启与窗口变化；最后一张在末轮正常退出前 |
| `view-menu-restored/recheck.jpeg` | “视图→缩放至全屏”位置和实际菜单，不与标题栏最大化混淆 |
| `*.memory.txt`、`*.top.txt` | `hidumper --mem PID` 与 `top -b -n 3 -d 1 -p PID` 离散采样，非FPS/交互峰值/泄漏测试 |
| `reports.json` | 包和输入哈希、恢复结果、真实GL JSON与内存/CPU采样解析；完整用户偏好未入库 |
| `receipt.json` | 本目录除receipt自身之外每个文件的字节数和SHA-256；`.gitattributes`禁止文本换行转换 |

文件名中斜线表示多文件命名模式，不是目录。`GL_FRAMEBUFFER`首次小尺寸来自预览视图；主视图正常深色尺寸为514×501和1356×893逻辑像素，System切换后尺寸会改变。

重放正常模型的设备命令（先安装报告中对应的签名HAP，再把仓库fixture发送到该沙箱目录）：

```text
hdc shell aa start -b io.github.xiaotaop.leocad -a QAbility --ps io.qt.appArgsJson '["/data/storage/el2/base/files/d5-fixtures/render-baseline.ldr"]'
hdc shell hilog -x -P <PID>
hdc shell hidumper --mem <PID>
hdc shell top -b -n 3 -d 1 -p <PID>
```

每次新进程PID重新采集；主窗口正常关闭后的native main返回0与启动日志配套。HDC命令耗时、截图抓取时刻都不等于首帧时长。调试HAP和用户配置备份在本地忽略的`build/`中，签名及设备唯一标识没有写入本目录。
