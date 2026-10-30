# D4 组长集成与真机验收

日期：2026-10-30（北京时间）；正式设备执行约 20:55–21:19。

**D4 组长交付完成，核心集成出口通过，带 P2 缺陷记录验收。** 包内零件库、模型和界面已在同一 HAP 可用；颜色缩略图有真实缺陷，不能声明全部 UI 无问题或达到发布验收。责任和后续安排见 [D4 缺陷表](D4_DEFECTS.md)。

2026-10-31 审查补记：原诊断入口在 `--version` / `--help` 提前成功退出时存在空库解引用，已由 `5b79fdd6` 修正，并完成宿主回归及 ARM64 编译。下列真机证据仍对应原包，未重写为修正提交的上机结果；范围见 [§9](#9-审核修正提前成功退出)。

## 1. 集成源码与包

- 三模块基线：`d5aa8b45104ab1bb47e18a54c73fb63005517413`，包含 A #24、C #23、B #25（以及先前的 B #22）。合并信息固定于 [merged-prs.json](evidence/leader/2026-10-30/d4-device/merged-prs.json)。没有将未合并分支叠入测试。
- D4 真机补丁：`fcb8fe55f1caa8473f6ceb261f41590ad5c61095`，仅在鸿蒙入口新增 `LEOCAD_RESOURCE_PATHS` JSON 日志，记录 Qt 实际资源路径、归档诊断和当前零件对象数。正常启动的设备记录如下；其提前退出回归与修正单列于§9。
- HAD-W32 / ARM64 / API23，OpenHarmony-6.1.0.115，Qt6.12.0，SDK26.0.0.105，OHOS Clang15.0.4，Maleoon916 / GLES3.2 B301 / GLSL ES3.20，DPR约1.9。
- 正常模式 `LEOCAD_SAFE_RENDER=OFF`；每轮 shader 均为 9 个、跳过 0 个。没有用隐藏网格的基础模式代替遮挡修复验收。
- 复用 `scripts/build-harmony-app.ps1 -GenerateSignature` 和现有官方调试签名；覆盖安装保留应用数据，未卸载或清空数据。

| 产物 | 源码 | 字节数 | SHA-256 |
|---|---|---:|---|
| 首轮集成主程序 | `d5aa8b45` | 80,056,337 | `907193d1b8e8c1db724ffe93d06231163ec7f5dd82ebc91f9cd0f6104a24727e` |
| 模型回归 HAP | `d5aa8b45` | 80,010,487 | `d731f467917f657d363a237492f9fcdd31682608c75461485b11a085098130e6` |
| D4 真机资源诊断包（审查修正前） | `fcb8fe55` | 80,061,743 | `2f44dd2cd6ea0ad0854e36732020856a96895c45a84075ba085e3f6c92d7b4bc` |

本地固定副本在 `build/harmony-d4-evidence/`；包和签名材料不提交。机器可读记录见 [builds.json](evidence/leader/2026-10-30/d4-device/builds.json)。

## 2. 组长 D4 清单

| 当日要求 | 实际结果 | 证据 |
|---|---|---|
| 合入三模块并统一构建 | main 集成版及入口诊断版均编译、签名、安装成功 | 上表与 merged-prs.json |
| 字体与菜单 | 中文主菜单、状态栏、偏好设置可读；文件菜单已没有导入/导出空容器；新串仍有英文，部分选项文字省略，记 P2 | [文件菜单](evidence/leader/2026-10-30/d4-device/file-menu.jpeg)、[搜索选项](evidence/leader/2026-10-30/d4-device/search-filter-options.jpeg) |
| 窗口缩放 | 恢复、最大化后继续绘制；计数与面板在两种尺寸下可见，FBO 深度未丢失 | [恢复窗口](evidence/leader/2026-10-30/d4-device/final-restored.jpeg)、[最大化](evidence/leader/2026-10-30/d4-device/final-maximized.jpeg) |
| 资源路径 | 确认 AppData、AppLocalData、Cache 的实际沙箱路径；包内 qrc 精简库加载正常 | final-normal.hilog.txt 与 reports.json |
| 核心模型与包内资源同时可用 | 正式两砖模型显示；分类和搜索可用；ARM64 模型测试 50/0/0 | [首屏](evidence/leader/2026-10-30/d4-device/normal-start.jpeg)、model-tests.txt |
| 维护缺陷表 | 记录缩略图颜色、偏好设置残留、文案/省略及已有平台/发布问题，注明负责人和阶段 | [缺陷表](D4_DEFECTS.md) |

## 3. A：正常模式网格与实际 FBO

按 [A D4 真机交接](MEMBER_A_D4_DEVICE_HANDOFF.md) 执行：原样使用 [render-baseline.ldr](evidence/leader/2026-10-30/d4-device/render-baseline.ldr)，通过既有沙箱路径和 Want 参数打开。不是系统文件选择器闭环验收。

- 默认视角下，网格在模型周围可见，红蓝实体表面不再有网格覆盖。
- 开启圆圈、网格线和原点轴后，实体仍遮挡参考网格；前视和 Home 视角可切换，选择着色及操纵器出现。
- 恢复与最大化两种尺寸均有实际 FBO 日志：`framebuffer == qt_framebuffer == 1`，`framebuffer_status == 36053`（完整），depth24/stencil8。主视图逻辑尺寸分别为 514×501、1356×893；DPR约1.9。首次预览视图的 200×173 不冒充主视图尺寸。
- 模型线宽设为3，经偏好设置关闭全部网格后，线宽仍为3；重新开启网格后继续可用。[线宽设置](evidence/leader/2026-10-30/d4-device/preferences-render.jpeg)、[关网格后的线宽](evidence/leader/2026-10-30/d4-device/linewidth-after-grid-off.jpeg)、[网格开启](evidence/leader/2026-10-30/d4-device/grid3-on-final.jpeg)、[网格关闭](evidence/leader/2026-10-30/d4-device/grid-off-linewidth3.jpeg)。本次没有额外采集驱动的宽线范围，不将设置值当作实际光栅线宽测量。
- 启动、正常关闭和再次启动均成功，native main 返回0；本次不使用强停作为退出判据。

`LEOCAD_GL_FRAMEBUFFER` 在状态恢复前采集，首次 `depth_test=false` 不视为失败；后续主视图日志也有 `depth_test=true`。实际深度附件和绑定以该日志为准，不单凭 context.format 的depth24推断。

截图是设备原始 JPEG。设备 `snapshot_display` 仅接受 `.jpeg`，没有伪造 PNG 或声称完成像素完全相同的自动对照。模型遮挡结果经逐图目视确认，宿主像素负例沿用 A 的提交；二者不是同一种证据。真实透明材质、专门贴图、上下文销毁重建、休眠及长期性能仍未在本轮逐项验收。

**#12 的正常模式真机复验条件已经满足，交 A 核对后决定关闭；本次未自动关闭远端 issue。**

## 4. C：面板、菜单与异常库

| 场景 | 真机结果 |
|---|---|
| 默认启动 | Brick 分类有缩略图，`Parts: 156` 完整可见 |
| All Parts | 两砖工程下显示1161；其中包含临时模型条目，不等同于包内库多出一个正式零件 |
| Sticker | 选中后为0，显示分类空态，文案完整 |
| 无结果搜索 | 显示搜索空态和0，清空可恢复 |
| `3001` 搜索 | 4项；在 Filter 子菜单关闭 Part ID 后为0，空态和计数同步；重新开启可恢复 |
| 文件菜单 | 没有 Import/Export 空子菜单；中文核心项可见 |
| 损坏候选归档 | 拒绝错误ZIP、使用内置128件兜底，缺件横幅出现；再搜索为空时仅横幅解释，不同时出现误导的搜索提示 |
| 损坏候选 + 正常外部库 | 使用用户偏好 `PartsLibrary` 指向同字节精简库；归档诊断仍保留，但默认 Brick156、无缺件横幅；空搜索仍有正常提示 |
| 健康候选 + 小外部库 | 偏好指向 `:/resources/library.zip`，没有归档诊断但实际降级；横幅仍出现，空列表有解释 |
| 颜色控件 | 点击后名称和色块改变；**缩略图红蓝通道表现异常，选Red显示蓝色，不通过，记P2** |

异常库没有改写嵌入 qrc：在 `FindBundledLibraryArchive()` 优先查找的沙箱候选放入37字节受控错误归档。外部库采用**用户偏好路径**，不是宣称设置了 Ability 的 `LEOCAD_LIB` 环境变量；两者均在源码中早于候选列表加载，但本轮直接验证的是前者。

设备查明：

```text
AppDataLocation      /data/storage/el2/base/files
AppLocalDataLocation /data/storage/el2/base/preferences
CacheLocation        /data/storage/el2/base/cache
正常包归档            :/assets/ldraw-lite/library-lite.zip
```

`LEOCAD_RESOURCE_PATHS.loaded_parts` 在 `Initialize()` 之后采集，包含当时的临时模型对象，正常可为1162、兜底可为131；不能用它推翻正式归档1160/128的校验，也不能与面板可见条目数直接相等比较。

缺件横幅的正常库回退、外部库优先和无诊断降级三条路径均直接在真机确认。未对所有31分类逐类计数；本轮确认了实际Brick、All Parts、Sticker以及搜索路径。新增英文提示、颜色名/分类名本地化仍按既有F-02在D7处理；筛选菜单中 `Regular...ression`、`显示...零件` 的省略列入同一UI质量跟进。

## 5. B：同一集成版本的 ARM64 模型回归

独立测试Bundle `io.github.xiaotaop.leocad.modeltests`，PID39458，50 passed / 0 failed / 0 skipped，测试报告出口0，同PID原生main返回0。

`editingSequenceUndoRedoAndSave`、`insertTwentyPiecesUndoRedoAndReopen` 均通过，覆盖插入、选择、移动、旋转、改色、删除、撤销重做及保存重开。测试使用合成固定零件和真实核心调用，**不替代正式GUI鼠标拾取或系统文件选择**。Windows专属锁测试和A新增的9组宿主线宽场景不在鸿蒙套件编译范围，不报为设备漏跑或skip。

原始 [model-tests.txt](evidence/leader/2026-10-30/d4-device/model-tests.txt) 和同PID [hilog](evidence/leader/2026-10-30/d4-device/model-tests.hilog.txt) 已归档。后续组长补丁只改 `qtmain.cpp`，该文件从独立模型测试目标中排除，未改变模型测试二进制的源码范围。既有D3真实离线编辑与MPD重开证据继续保留，本轮不重复声称完整GUI闭环已重新验收。

## 6. 运行与恢复

| 用途 | PID | native main |
|---|---:|---|
| main集成首轮 / 两次重启 | 38187 / 39676 / 39769 | 均0 |
| 网格、线宽和窗口 | 40043 | 0 |
| 诊断版正常库 | 43650 | 0 |
| 坏候选归档回退 | 43858 | 0 |
| 坏候选 + 正常外部库 | 44934 | 0 |
| 健康候选 + 有效小库 | 47544 | 0 |
| 恢复正常配置及窗口 | 47669 | 0 |
| 颜色缺陷补查 | 48248 | 0 |

准备阶段曾因HDC路径/权限及目录传输行为导致候选未命中、外部文件未送达；40847、41327、44173、44613、45411等准备/恢复过程**不作为异常库测试通过轮次**。其中不存在的外部库偏好触发了旧版启动模态提示，保留日志并归入#13后续排查；没有把它写成“默认Ability问题已修复”。

恢复时HDC shell不能移动应用所有的候选文件；使用一次本地临时构建，仅在文件全部37字节等于受控错误内容时，将两处测试候选改名为非加载名称。该构建PID47073，两次改名均true，main返回0。其补丁和HAP只留本地 `build/`，没有提交或保留在交付入口中；它不计作正式验收包。随后覆盖安装先前固定哈希的 `fcb8fe55` 正常包，正式完成小库负例和最终正常轮次。

结束时已装回正常OFF包、确认从qrc加载且无库诊断、正常关闭应用。用户原偏好文件按原字节恢复，前后SHA-256一致；没有覆盖固定模型、卸载应用或清空用户数据。有效外部测试库和已失活错误样本仍留在明确的测试目录，均不在最终加载路径；无需用户手工恢复配置。恢复收据见 [restoration.json](evidence/leader/2026-10-30/d4-device/restoration.json)。

## 7. 验收边界

组长当日工作是集成、设备执行、入口诊断和缺陷/证据交付，已完成。当前未观察到阻断这一阶段出口的新P0/P1；缩略图颜色和偏好设置裁剪等P2必须继续跟进，不将“可交付D4”写成“可以上架”。

系统文件选择、外部授权/导出/取消与失败、完整GUI文件闭环、长时间稳定性、正式签名及许可/上架材料仍按原计划处理。没有因为本次出现原生文件选择窗口就关闭#15，也没有关闭#13/#14/#16。

全部原始证据：[d4-device](evidence/leader/2026-10-30/d4-device/)。[reports.json](evidence/leader/2026-10-30/d4-device/reports.json) 从原始hilog抽取；[receipt.json](evidence/leader/2026-10-30/d4-device/receipt.json) 固定证据字节与哈希，不包含自己。

## 8. GitHub 交付与跟踪

工作分支 `feature/leader-d4-device` 已推送并创建 [PR #26](https://github.com/xiaotaop/leocad-harmony/pull/26)。新增 [#27 缩略图颜色异常](https://github.com/xiaotaop/leocad-harmony/issues/27)（A主责、C配合，D5优先）与 [#28 中文文案及文字省略](https://github.com/xiaotaop/leocad-harmony/issues/28)（C，D7）；偏好设置裁剪继续使用 [#14](https://github.com/xiaotaop/leocad-harmony/issues/14)，没有重复建单。

#12 补充正常模式真机通过、待 A 核对；#13 补充异常偏好准备过程和恢复边界；#15 补充同一集成基线的50项回归及剩余系统闭环。以上 issue 继续开放，#16 保持原安排。本次没有主动指定 PR 审核人或执行合并，仓库分支保护继续生效。

## 9. 审核修正：提前成功退出

成员 A 对 `101ed826` 的[审核](https://github.com/xiaotaop/leocad-harmony/pull/26#pullrequestreview-5715021051)指出：`Initialize()` 遇 `Options.Exit` 返回 `Success` 时尚未创建零件库，旧日志无条件读取对象数会崩溃。`5b79fdd6d2e2a64f8baa06d7be0bae699b60d1e3` 先读取库指针，存在时保留原计数，不存在时将 `loaded_parts` 记为 JSON `null`。无效参数的错误出口保持返回1。

宿主验证只通过默认关闭的 `LEOCAD_TEST_RESOURCE_PATHS=ON` 启用生产日志块及其 includes，未定义 `Q_OS_HARMONY` 或启用其他鸿蒙窗口/GL逻辑。修复前完整应用在 GDB 下执行 `--version`、`--help`，均输出请求文本后发生 SIGSEGV；修复后完整应用 CTest 的三条子进程检查均通过：两条成功出口返回0、各有一条 `loaded_parts:null` 日志，无效参数返回1且无该日志。CI桌面构建现已执行同一回归，必须存在日志才能通过，避免因未编译鸿蒙诊断块而假通过。

另外完成正常 `Q_OS_HARMONY` 分支的 ARM64 `leocad` 目标编译/链接，测试开关为OFF。**本次没有重新打包、签名、安装或执行真机**；新 `.so` 哈希不能作为新HAP哈希。原D4的64项证据和收据保持不变，宿主测试不替代修正版的真机生命周期结果。模型业务/渲染未改，#27/#14/#28等问题继续独立跟进。

本次复现、构建、回归原始记录与源码/产物哈希：[d4-startup-review](evidence/leader/2026-10-31/d4-startup-review/)。复现命令与边界见 [启动回归说明](../tests/startup/README.md)。
