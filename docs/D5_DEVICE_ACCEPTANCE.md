# D5 组长真机验收

日期：2026-10-31（北京时间），正式操作约 15:24–15:40。

**组长 D5 验证与记录工作完成；固定模型稳定显示的阶段出口通过。** 本轮没有录屏，按组长要求保留截图、原始日志和哈希。颜色修复与偏好设置裁剪已上机；前视适应窗口和 System 主题仍有缺陷，不能据此宣称 A/C 全部验收通过或可发布。责任与复现见 [D5 缺陷表](D5_DEFECTS.md)。

## 1. 源码、设备与安装包

| 项目 | 本轮实际值 |
|---|---|
| 集成源码 | `f7a83c5eff03c5289ea60d7bac83caf92ba6210d`，main 已包含 #26、#29、#30、#31 |
| 设备 | HAD-W32，ARM64，OpenHarmony-6.1.0.115，API23，3120×2080，DPR约1.9 |
| 图形 | Maleoon 916，OpenGL ES 3.2 B301，GLSL ES 3.20 |
| 工具 | Qt 6.12.0；DevEco CLI Tools 26.0.0.851；SDK 26.0.0.105；OHOS Clang 15.0.4，版本固定于 `toolchain.lock` |
| 构建 | Release；`LEOCAD_SAFE_RENDER=OFF`；`LEOCAD_TEST_RESOURCE_PATHS=OFF` |
| 签名 HAP | `build/harmony-d5-evidence/leocad-f7a83c5e-normal-signed.hap`，80,064,963 字节 |
| HAP SHA-256 | `f02b9fbc36e308a33b17f7ccd3f0093961020a281b7be9c849e211d5a1a6aeb8` |

使用 `scripts/build-harmony-app.ps1 -BuildDirectory build/harmony-d4 -GenerateSignature` 增量构建，hvigor 返回 BUILD SUCCESSFUL，随后 `hdc install -r` 覆盖安装。复用构建目录名称不表示沿用 D4 二进制；上表是本轮构建后固定副本的哈希。包、签名和完整用户偏好不入库。

输入为原样 [render-baseline.ldr](../tests/fixtures/render-baseline.ldr)，267字节，SHA-256：`25a650cddc8a6da11ae8edac3f71328294308244ea1280eaa9a78fbd88a1769b`。通过 HDC 放入独立沙箱目录 `files/d5-fixtures/`，用 Want 的 `io.qt.appArgsJson` 打开。结束时重新导出并核对，同一哈希；没有保存插入测试到该文件。此路径不替代系统文件选择器验收。

机器可读结果见 [reports.json](evidence/leader/2026-10-31/d5-device/reports.json)，逐文件校验见 [receipt.json](evidence/leader/2026-10-31/d5-device/receipt.json)。

## 2. 组长当日交付

| D5 要求 | 结果与边界 |
|---|---|
| 真机加载固定模型 | **通过**：正常模式显示原有红蓝两砖，缩放窗口后仍可绘制；[最大化](evidence/leader/2026-10-31/d5-device/baseline-maximized.jpeg)、[最终恢复窗口](evidence/leader/2026-10-31/d5-device/final-restored.jpeg) |
| 渲染错误采集 | 保存完整进程日志；9个shader程序通过、跳过0；实际FBO完整，depth24/stencil8；Qt/Ace窗口回收告警保留并单列，未写成“日志零错误” |
| 性能采样 | 固定模型空闲、颜色操作后、偏好窗口打开、重启后采集 `hidumper --mem` 和 `top`；数据见§6，不冒充FPS或长期稳定性测试 |
| 是否关闭额外效果 | **保留正常模式 OFF**，不强制切基础包。默认模型能显示、实体遮挡网格，当前没有需要通过基础模式规避的渲染阻断；不据此声称全部材质或高级效果已验收 |
| 录屏 | **按组长要求取消**，计划当日行已同步改为截图与日志 |
| 缺陷交接 | 原颜色问题复验通过；记录前视适应窗口、System主题两项剩余问题；没有自动关闭远端issue |

## 3. A：颜色、窗口、相机与重启

按 [A D5 真机交接](MEMBER_A_D5_DEVICE_HANDOFF.md) 使用本轮同一正常包：Brick分类搜索 `3001`，选择第二个 `3001.dat`（Brick 2×4），分别切换 Red、Blue、Light Blue。

| 检查 | 结果 | 截图 |
|---|---|---|
| Red 名称/色块/缩略图/插入实体 | 红色一致；原模型的红蓝未互换 | [最大化](evidence/leader/2026-10-31/d5-device/red-inserted-maximized.jpeg)、[恢复窗口](evidence/leader/2026-10-31/d5-device/red-inserted-restored.jpeg) |
| Blue 名称/色块/缩略图/插入实体 | 蓝色一致 | [最大化](evidence/leader/2026-10-31/d5-device/blue-inserted-maximized.jpeg)、[恢复窗口](evidence/leader/2026-10-31/d5-device/blue-inserted-restored.jpeg) |
| Light Blue 名称/色块/缩略图/插入实体 | 浅蓝一致，没有D4偏黄现象 | [最大化](evidence/leader/2026-10-31/d5-device/lightblue-inserted-maximized.jpeg)、[恢复窗口](evidence/leader/2026-10-31/d5-device/lightblue-inserted-restored.jpeg) |
| 三色逐项撤销 | 每次均撤回到两砖基线，标题无修改星号；不同颜色之间没有累积插入 | 对应 `red/blue/lightblue-undone-maximized/restored.jpeg` |
| 窗口变化 | 主视图逻辑尺寸514×501与1356×893，DPR约1.9；FBO完整，继续绘制；恢复窗口下新增2×4可能超出旧相机取景，颜色证据不作适应窗口证据 | 原始 `normal-main.hilog.txt` |
| 前视/顶视/Home切换 | 视角均能切换；每次再点“视图→缩放至全屏”（ZoomExtents） | `front/top/home-fit-maximized/restored.jpeg` |
| ZoomExtents完整取景 | **部分失败**：顶视、Home可见完整两砖；前视左右有裁切，重启后及两种尺寸均复现 | [前视最大化](evidence/leader/2026-10-31/d5-device/restart-front-fit-maximized.jpeg)、[前视恢复窗口](evidence/leader/2026-10-31/d5-device/restart-front-fit-restored.jpeg) |
| 正常关闭并重启 | 成功；新PID14905下再切三色和三个视角，颜色仍正确，前视裁切仍在 | `restart-red/blue/lightblue-maximized.jpeg`、`restart-front/top/home-fit-maximized/restored.jpeg`、`restart-dark.hilog.txt` |

插入通过键盘 Insert 调用当前零件的插入命令，点空白取消选择后查看真实颜色。点空白会产生选择历史，因此本轮对该序列执行两次撤销（取消选择记录、插入记录）恢复基线；没有把第一次撤销只改变选择误报为撤销失效。本轮不声称完成双击后鼠标定位放置、拖放或拾取精度验收。

首次读回日志：`source_format=18`、`source_depth=32`、`source_stride=972`、243×243，转换后 `destination_format=5`。这是真实设备源格式，不是预填“必为RGBA8888”；与三色截图共同支持 #27 的颜色修复。原始 [normal-main.hilog.txt](evidence/leader/2026-10-31/d5-device/normal-main.hilog.txt) 保留首个读回、GL上下文、shader、资源及窗口FBO记录。

FBO绑定 `framebuffer=qt_framebuffer=1`，status36053（完整），depth24/stencil8。首次状态恢复前 `depth_test=false`，随后主视图为true；没有仅凭请求的context.format推断附件。包内库实际来源为 `:/assets/ldraw-lite/library-lite.zip`，`bundled_problem` 为空；`loaded_parts=1162` 包含临时模型对象，不能当作正式精简库的零件数量。

**#27 的三色真机复验通过，可交A确认；相机完整取景不通过，不能写成A整份交接全部通过。**

## 4. C：偏好设置裁剪与保存

| 检查 | 真机结果 |
|---|---|
| 常规页移除项 | 无检查更新、外部零件库路径、POV-Ray、LGEO；[常规页](evidence/leader/2026-10-31/d5-device/preferences-general.jpeg) |
| 常规页保留项 | 颜色配置、人仔设置、语言、主题、作者及必要选项仍在，行标题对应，无原隐藏项残留空行 |
| 渲染页 | 无Blender设置，Lines及线宽、着色/渲染相关项仍可见；[渲染页](evidence/leader/2026-10-31/d5-device/preferences-rendering.jpeg) |
| 设置保存 | Dark改为System，颜色重置询问选择No，再点OK；沙箱配置写入 `ColorTheme=1`，正常关闭后新进程14319仍为1；样式确实变化 |
| System样式可用性 | **失败**：主菜单文字与当前颜色名不可见，重启后仍复现；[保存后](evidence/leader/2026-10-31/d5-device/system-theme-applied.jpeg)、[重启后](evidence/leader/2026-10-31/d5-device/restart-system.jpeg) |
| 原配置恢复及再次打开 | 完整原偏好恢复后，PID14905重开偏好对话框成功，显示深色/默认语言；[恢复后再次打开](evidence/leader/2026-10-31/d5-device/preferences-reopened-dark.jpeg) |

本轮只证明上述 **System + 不重置颜色** 序列，不推断其他选择均失败，也没有把样式失败解释为裁剪代码必然引入的回归。设置持久化成功不等于该主题可用。

**#14 原有的设置残留已在当前包消失；C文档§7“设置保存后正常可用”不能整项勾通过，System主题需跟进。**

## 5. 进程和恢复

| 用途 | PID | native main返回 |
|---|---:|---:|
| 正式三色、窗口、相机、偏好测试 | 10733 | 0 |
| System主题持久化重启 | 14319 | 0 |
| 恢复传输路径校正前的额外短轮次 | 14846 | 0，仍为System；不作深色恢复证据 |
| 原偏好恢复后，重启复验 | 14905 | 0 |

前三个正式证据进程的原始日志分别为 `normal-main.hilog.txt`、`restart-system.hilog.txt`、`restart-dark.hilog.txt`。额外短轮次日志保留在本地忽略目录，不混入正式深色复验。

没有使用强停代替正常退出。每轮点击窗口关闭按钮后，日志出现native main返回0；末轮关闭后进程列表不再有应用进程。应用保留本轮正常模式签名包。

偏好在测试前备份，测试结束、进程退出后完整写回并重新导出验证。前后均为3357字节、SHA-256 `452a53ac671c264a8232d2f33c8c996204e6f0ad80696f7fe7caa8d17c6d8a8a`。原文件内容未入库。固定模型的结束哈希也与输入相同；HDC专用样例目录保留用于复现。

## 6. 性能数据与日志限制

`hidumper --mem PID` 的 `Pss Total` 单位为kB，以下按1024换算为MiB；不是RES或VIRT。

| 状态 | PID | PSS kB | 约MiB |
|---|---:|---:|---:|
| 两砖基线空闲 | 10733 | 303208 | 296.1 |
| 三色操作后，恢复窗口 | 10733 | 276334 | 269.9 |
| 偏好窗口打开 | 10733 | 374410 | 365.6 |
| 原配置重启后，最大化 | 14905 | 324513 | 316.9 |
| 结束前，恢复窗口 | 14905 | 281174 | 274.6 |

每个状态保留 `.memory.txt` 和三次1秒间隔的 `.top.txt`。基线空闲采样应用CPU为0.0%、0.0%、1.0%，RES约311–312M；数据只说明这些离散空闲点。本轮没有可靠FPS、交互峰值、完整首帧时长、复杂工程性能或长时间泄漏结论。`aa start` 命令耗时不当作首帧耗时。

GL诊断通过，日志中未检出 `FATAL`、`SIGSEGV`、`GL_INVALID` 或 `GL_OUT_OF_MEMORY`。但存在Qt OHOS `xComponent ... does not have any assigned receiver`、Ace `pipeline context is null, OnInactive failed` 等窗口回收日志，原样保留；该轮仍可继续操作且正常退出，原因尚未单独定位，不能写成“所有运行日志零错误”。

## 7. 本轮边界

本次仅增加组长验收资料、缺陷交接及录屏要求调整，没有修改 A/C 业务代码，没有重跑或冒认 B 的宿主65项或D4 ARM64模型50项为今日新设备结果。系统选择器、外部授权读写、完整GUI保存重开、全部材质、休眠恢复及发布权限仍按原阶段推进。

今天的组长任务可以交付；后续由A修复前视适应窗口、C处理System主题。组长后续要求提交远端：已建立 [#32 前视裁切](https://github.com/xiaotaop/leocad-harmony/issues/32) 并指派A（zycAlmira），建立 [#33 System主题](https://github.com/xiaotaop/leocad-harmony/issues/33) 并指派C（Leaf-21）；本次资料通过组长D5验收PR提交，没有自动关闭旧issue。
