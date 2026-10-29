# D3 合入后资源与基础编辑真机补验

日期：2026-10-29，约 21:00 统一批次。来源：C 的 PR #17/#19 交接清单；B 的 #18 不新增真机用例。

后续更新：当晚已补 HAP 图标配置并另包验证通过，见 [图标修复验收](HAP_ICON_DEVICE_ACCEPTANCE.md)。下文保留本轮原始基线与发现，不能用后续新包结果覆盖旧包收据。

**结论：精简库实际加载、MPD 切换、离线基础编辑和退出重启通过；不能将 C 的整张交接清单标为全部通过。菜单分组和系统图标仍需修正，分类总数显示要求尚未完成设备核验。**

## 固定源码、产物与环境

- 源码：`fbcdc5e11be8a9dc46471b8ac0990cb603982740`，main 已包含 #11/#17/#18/#19。本次没有改业务代码。
- 设备：HUAWEI HAD-W32、ARM64、API23；Qt 6.12.0，Maleoon 916 / GLES 3.2 B301 / GLSL ES 3.20。
- 构建：`scripts/build-harmony-app.ps1 -SdkRoot <本机SDK> -QtRoot <Harmony Qt Kit> -BuildDirectory <仓库>/build/harmony-d3 -GenerateSignature`。
- 官方调试签名 HAP：79,720,294 字节，SHA-256 `d1a7f94f036087aa10650bc05d9aa9ce84ac6497e01430ba36155f292502daa7`。
- 本机原始包：`build/harmony-d3/harmonyos-build/entry/build/default/outputs/default/entry-default-signed.hap`；另保留固定命名副本于 `build/harmony-d3-evidence/`。
- Bundle `io.github.xiaotaop.leocad`，Ability `QAbility`。真实编译命令含 `LC_DISABLE_NETWORK=1` 和 `LC_DISABLE_NON_CORE_FORMATS=1`。
- 本次为覆盖安装，保留了此前的偏好和沙箱；没有宣称完成清除数据后的首次安装验收或精确冷启动性能测量。
- 原始证据目录：[d3-resources](evidence/leader/2026-10-29/d3-resources/)，文件哈希见其中 `receipt.json`。不提交 HAP、签名材料或完整设备标识。

## 实测结果

| 项目 | 结果与证据 |
|---|---|
| 主窗口启动、正常退出、二次启动 | PID 9047、9110、10033；三轮均由窗口关闭，原始日志均有 `libleocad.so ... returned 0`，未用强停代替正常退出 |
| 精简库实际加载 | `curated-device.mpd` 使用内置小库没有的 `10172.dat` 和 `30293.dat`，真机均显示真实几何而非占位方块，见 `mpd-main.jpeg`；三轮均无 `Bundled parts library rejected:` |
| 1160 件与分类显示 | 包内 manifest 为 1160 顶层件、669 子件、911 图元、1 配置。本次以独有件证明运行时选中精简库；界面没有显示全库总数，未直接读取进程内总数，也未逐一枚举设备上的 31 个分类节点，不能把 manifest 统计当成 UI 计数验收 |
| MPD 切换 | 子模型菜单→子模型对话框→选择 child→OK，显示 child 几何并新增 child 标签；点击 main 标签可切回。无新建/删除/重命名/复制/导出按钮，仅保留激活切换；截图 `model-list.jpeg`、`mpd-child-active.jpeg` |
| 标签结构保护 | 无标签关闭按钮；拖动 child 到 main 前，顺序未改变，见 `tabs-unchanged.jpeg` |
| 菜单精简 | 打印/渲染等直接入口已移除；但“导入”“导出”分组仍在文件菜单中可见，与交接清单“看不到导出项”不符，见 `file-menu.jpeg`；本次没有据此声称非核心导出重新可用 |
| 图标 | 关于对话框中的 LeoCAD 红色积木资源正常；系统主窗口、对话框标题栏和任务栏仍显示 Qt 默认蓝色图标，不能验为系统应用图标接入通过，见 `about.jpeg`、`ready.jpeg` |
| WLAN 关闭后的基础编辑 | 系统控制中心关闭 WLAN（`wifi-off.jpeg`），通过鼠标选择零件和键盘操作完成插入、移动、旋转、删除、撤销、Ctrl+S 沙箱保存。关闭后仍能重启、读取保存结果，见 `offline-reopen.jpeg` |
| 网络行为边界 | 编译隔离宏存在，关闭 WLAN 后无联网阻塞；未抓包或审计所有 socket，不能将本次结果表述为“证明没有任何网络请求”。测试结束恢复 WLAN 开启，应用正常关闭 |

`sticker.jpeg` 仅记录一次受系统中文输入法预编辑影响的输入尝试，没有成功确认 Sticker 分类为空，不算通过证据。`start.jpeg` 是启动过渡画面，主窗口就绪以 `ready.jpeg` 为准；`offline-insert.jpeg` 是插入前的尝试，以 `offline-insert2.jpeg` 和实际 MPD 为准。`mpd-child.jpeg` 是选择 child 后尚未点击 OK 的对话框，以 `mpd-child-active.jpeg` 为切换成功证据。

## 离线编辑的数据证据

使用隔离的测试文档 `curated-device.mpd`，通过 HDC 放入应用沙箱；使用 Want 的 `io.qt.appArgsJson` 冷启动传入路径，**不是系统文件选择器验证**。每步通过 UI 保存，再从应用沙箱取回原文件：

1. `after-insert.mpd`：main 增加一件 `3005.dat`，坐标 `(45,-24,0)`，单位矩阵。
2. `after-move.mpd`：同一件坐标变为 `(45,-24,-10)`。
3. `after-rotate.mpd`：同一件矩阵变为 `0.866025 0.5 0 / -0.5 0.866025 0 / 0 0 1`。
4. `after-delete.mpd`：该件记录消失，原有两件独有零件及 MPD 结构保留。
5. `after-undo.mpd`：与 `after-rotate.mpd` 逐字节一致。
6. 正常退出 PID 9110，在 WLAN 仍关闭时启动 PID 10033，实际看到保存后的三个零件与旋转姿态。

固定操作：Insert 插入；方向右键移动；Shift+方向右键旋转；Delete 删除；Ctrl+Z 撤销；每步 Ctrl+S。未修改模型断言或用主机生成编辑结果冒充设备操作。

## 待处理与范围

- C：清理空的导入/导出菜单容器，补真实菜单可见性验证。
- 组长平台打包 + C 图标资源：将应用图标接入 Harmony HAP 元数据，不能只依赖 Qt `setWindowIcon()`。
- C/组长后续资源批次：明确 1160 总数是否需要直接展示，补分类节点总数和 Sticker 空分类的设备核对。
- A：旧 Issue #12 的网格覆盖模型表面仍存在；本次未修复，不认定整体渲染质量通过。
- 系统选择器、授权、外部导出、取消和错误路径仍按 Issue #15 在 D7–D8 完整验收；本次只补了其中可独立完成的沙箱离线编辑/重开路径。
- B 的 #18 复用既有 ARM64 50/0/0 证据，本次不重复跑相同模型测试，也不把旧包结果扩展为新包全量 API 验收。

这些差异不撤销已通过的 D3 主窗口出口，也不代表上述整张资源交接清单已经全部关闭。本轮证据随图标修复分支提交，未新增远端 issue。
