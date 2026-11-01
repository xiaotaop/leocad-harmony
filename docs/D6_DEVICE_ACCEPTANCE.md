# D6 组长集成与真机验收

日期：2026-11-01（北京时间），实际应用运行约 19:53–20:15。

**D6 核心出口“选零件→插入→移动→保存前状态”通过。** 已从合入 A/B/C 的 main 重新构建、签名、安装，完成面板、颜色、移动历史及失焦取消验证。A 的完整交接仍有补测项：Shift/Alt + 右键组合、严格棱线拾取及最小化路径，不能将本报告解读为交接九步全部通过。按组长要求没有录屏。

本轮操作通过 HDC 的 UITest/uinput 在已连接的实体设备上执行；这是设备注入输入验证，尚不能代替物理键鼠组合操作确认。没有修改业务代码，没有重跑或冒认各成员的宿主测试。

## 1. 被测版本与构建

| 项目 | 本轮实际值 |
|---|---|
| 集成源码 | `5ade519591bf6931b5631593d34455882c10f490`，包含 A #36、B #38、C #37 |
| 设备参数读回 | HAD-W24；`6.1.0.117(SP68C00E100R13P3)`；API23；3120×2080 |
| 图形 | Maleoon 916；OpenGL ES 3.2 B301；GLSL ES 3.20；DPR约1.9 |
| Qt / CLI | Qt 6.12.0 Harmony ARM64；DevEco CLI Tools 26.0.0.851 |
| 构建开关 | Release；`LEOCAD_SAFE_RENDER=OFF`；`LEOCAD_TEST_RESOURCE_PATHS=OFF` |
| 新签名包固定副本 | `build/d6-device-package/leocad-5ade5195-signed.hap`，80,094,554字节 |
| HAP SHA-256 | `6448cf5f3118ae26b860525b621863cfd61800dd3aa16848f7c33c197f53f68a` |

执行 `scripts/build-harmony-app.ps1 -BuildDirectory build/harmony-d4 -GenerateSignature`，native 编译及 hvigor 构建成功，再用 `hdc install -r` 安装。复用构建目录不表示复用旧包。签名、安装包、完整偏好备份及构建日志留在被忽略的 `build/`。

输入为原样 [render-baseline.ldr](../tests/fixtures/render-baseline.ldr)，267字节，SHA-256 `25a650cddc8a6da11ae8edac3f71328294308244ea1280eaa9a78fbd88a1769b`。使用现有沙箱 `files/d5-fixtures/`，通过 Want 参数 `io.qt.appArgsJson` 打开；不据此声称系统文件选择器或外部文件读写通过。

机器可读记录：[reports.json](evidence/leader/2026-11-01/d6-device/reports.json)；原始证据哈希：[receipt.json](evidence/leader/2026-11-01/d6-device/receipt.json)。

## 2. 组长核心闭环及 B 编辑集成

| 操作 | 实际结果 | 证据 |
|---|---|---|
| 选零件 | All Parts 搜索 `3003`，选择普通 `3003.dat`，进入插入命令 | [零件搜索](evidence/leader/2026-11-01/d6-device/part-3003-search.jpeg)、[插入菜单](evidence/leader/2026-11-01/d6-device/insert-menu-before-click.jpeg) |
| 插入 | 新件出现在蓝砖上方，选中位置 `(30,0,24)`，标题出现修改星号 | [插入后](evidence/leader/2026-11-01/d6-device/inserted-green-confirm.jpeg) |
| 鼠标移动并松开 | 拖动移动轴，位置变为 `(40,0,24)` | [移动后](evidence/leader/2026-11-01/d6-device/move-green.jpeg) |
| 撤销 / 重做 | Ctrl+Z 回到 `(30,0,24)`，Ctrl+Y 回到 `(40,0,24)`；菜单显示移动历史 | [撤销](evidence/leader/2026-11-01/d6-device/move-undo.jpeg)、[重做](evidence/leader/2026-11-01/d6-device/move-redo.jpeg)、[历史名称](evidence/leader/2026-11-01/d6-device/edit-menu-after-move.jpeg) |
| 保存前状态 | 编辑后文档有星号，关闭时出现保存询问；选 No 丢弃测试编辑 | [关闭确认](evidence/leader/2026-11-01/d6-device/close-unsaved-prompt.jpeg) |

这是实际几何坐标变化和移动历史验证，没有把选择/取消选择当作移动。没有保存测试编辑到固定样例；GUI 保存重开闭环按后续阶段验证。

## 3. C 零件面板与颜色

对照 [C D6 数据链交接](D6_零件面板数据链.md)：

| 场景 | 实际结果 | 证据 |
|---|---|---|
| 首屏 | Brick，156件，面板非空 | [启动](evidence/leader/2026-11-01/d6-device/baseline-start.jpeg) |
| Sticker | 0件，提示当前分类没有零件 | [空分类](evidence/leader/2026-11-01/d6-device/sticker-empty.jpeg) |
| Sticker + `10172` | 仍提示分类没有零件，没有错误归因于搜索 | [组合空态](evidence/leader/2026-11-01/d6-device/sticker-with-search.jpeg) |
| All Parts + 不存在词 | 0件，提示没有零件匹配搜索 | [无匹配](evidence/leader/2026-11-01/d6-device/search-no-match.jpeg) |
| All Parts + `10172` | 1件，可见杯形零件缩略图 | [搜索命中](evidence/leader/2026-11-01/d6-device/search-10172.jpeg) |
| 清空搜索 / 分类恢复 | 清空后 All Parts 显示1161件；第二次启动 Brick 恢复156件 | [全部件恢复](evidence/leader/2026-11-01/d6-device/allparts-restored.jpeg)、[重启面板](evidence/leader/2026-11-01/d6-device/restart-pick-red.jpeg) |
| Green 颜色链 | 颜色名、色块和面板缩略图同步；另插入一个绿色1×1零件，取消选中后实体仍为绿色，原红蓝两件未改色 | [颜色控件](evidence/leader/2026-11-01/d6-device/color-green.jpeg)、[实体颜色](evidence/leader/2026-11-01/d6-device/green-insert-full.jpeg) |

已补上 C 交接需要的设备截图。分类计数恢复有上述场景证据；未将重新启动代替“同一进程 Brick→All Parts”的专门往返测试。

## 4. A 拾取、导航、焦点与窗口

对照 [A D6 真机交接](MEMBER_A_D6_DEVICE_HANDOFF.md)：

| 检查 | 结果与边界 |
|---|---|
| 最大化红/蓝实体拾取 | 红砖选中 `(-30,0,0)`；显式切回“编辑→工具→选择”后蓝砖选中 `(30,0,0)`。见 [红砖](evidence/leader/2026-11-01/d6-device/restart-pick-red.jpeg)、[蓝砖](evidence/leader/2026-11-01/d6-device/blue-select-tool.jpeg) |
| 默认右键旋转 | 右拖后视角改变，松开后的无按钮移动没有继续旋转。见 [旋转](evidence/leader/2026-11-01/d6-device/navigation-orbit.jpeg)、[松开后](evidence/leader/2026-11-01/d6-device/navigation-released.jpeg) |
| 平移 / 缩放工具 | 显式选择平移工具后拖动，模型整体平移、方向保持；缩放工具拖动后模型大小改变。见 [平移前](evidence/leader/2026-11-01/d6-device/pan-tool-before.jpeg)、[平移后](evidence/leader/2026-11-01/d6-device/pan-tool-after.jpeg)、[缩放后](evidence/leader/2026-11-01/d6-device/zoom-tool-after.jpeg) |
| 移动中失活取消 | 按住移动轴产生 `(-30,10,0)` 预览；点击系统设置的 Dock 图标，应用离开前台，在外部松开，再用 `aa start` 返回原进程。零件回到 `(-30,0,0)`，选择保留，标题无修改星号。见 [移动预览](evidence/leader/2026-11-01/d6-device/focus2-move-preview.jpeg)、[返回取消](evidence/leader/2026-11-01/d6-device/focus2-cancel-return.jpeg) |
| 返回后的首次新移动 | 无预先悬停，直接同一轴拖动，提交 `(-30,10,0)`，Ctrl+Z 恢复 `(-30,0,0)`。见 [新移动](evidence/leader/2026-11-01/d6-device/focus2-new-move.jpeg)、[撤销](evidence/leader/2026-11-01/d6-device/focus2-undo.jpeg) |
| 右键导航中断 | 同样切到设置、外部松开、返回；无迟到右键菜单，随后无按钮移动不继续旋转。见 [返回](evidence/leader/2026-11-01/d6-device/orbit-interrupt-return.jpeg)、[自由移动](evidence/leader/2026-11-01/d6-device/orbit-interrupt-free-move.jpeg)。普通右键点击尝试的截图没有显示菜单，不能确认该子项通过，需物理右键对照 |
| 区域缩放中断 | 出现缩放框后切换应用、外部松开再返回，框消失，取景未按该框执行缩放。见 [操作前](evidence/leader/2026-11-01/d6-device/region-before.jpeg)、[预览框](evidence/leader/2026-11-01/d6-device/region-preview.jpeg)、[取消返回](evidence/leader/2026-11-01/d6-device/region-cancel-return.jpeg) |
| 视角球中断 | 在球上尝试按住拖动、切换应用、外部松开再返回；[返回](evidence/leader/2026-11-01/d6-device/sphere-cancel-return.jpeg)与[后续移动](evidence/leader/2026-11-01/d6-device/sphere-free-move.jpeg)画面一致。但没有保留可确认球已进入旋转的中间画面，不能据此验收视角球中断，标待补测 |
| 恢复窗口 | 恢复窗下可拾取红/蓝；蓝砖从 `(30,0,0)` 移动到 `(40,0,0)`，撤销回到 `(30,0,0)`；右拖后视角改变。见 [红砖](evidence/leader/2026-11-01/d6-device/restored-final-red.jpeg)、[蓝砖](evidence/leader/2026-11-01/d6-device/restored-final-blue.jpeg)、[撤销后](evidence/leader/2026-11-01/d6-device/restored-blue-undo.jpeg)、[旋转后](evidence/leader/2026-11-01/d6-device/restored-orbit.jpeg)。移动中截图有系统网络通知，留在本地探索档案，不收入本次证据 |
| 关闭 / 重启 | 首轮 PID20566 正常退出，新 PID23172 重启后完成拾取、导航、失焦及移动验证，最终也正常退出 |

实际日志 DPR为 `1.899999976158142`。主视图最大化逻辑尺寸1374×893、恢复窗532×501；对应渲染目标约2611×1697和1011×952物理像素。启动早期另有200×173、514×501的初始化尺寸，不能把它们混作最终窗口尺寸。

### 未计作通过的尝试与待补项

- **Shift+右拖平移、Alt+右拖缩放：待物理键鼠确认。** 分开的键盘与鼠标注入未能证明修饰键同时送达 Qt，画面仍表现为旋转；显式工具平移、缩放已通过。不能把工具验证替代组合键，也暂不把该注入结果定为产品缺陷。责任交接 A/组长。
- **棱线附近的精确重复拾取、最小化/Hide路径：未专门覆盖。** 已完成实体内部拾取、最大化/恢复窗与系统设置切换失活。责任交接 A/组长。
- **普通右键菜单、视角球激活后的中断：证据不足。** 右键点击截图未见菜单；球的尝试缺少操作激活证据，不能用前后相同画面直接标通过。责任交接 A/组长。
- 早先 Alt+Tab 尝试未确认实际切换应用，不纳入失焦结果；后续使用设置窗口完成了有效序列。
- 区域缩放测试后仍处于该工具，普通点击不能用作拾取判据；切回“选择”后重新取得蓝砖证据。早先坐标准备及工具选择不正确的截图未收入正式证据。

因此核心出口可交付；A 扩展清单尚不能标全绿。后续补测按上述明确步骤继续，不要求重复已通过的核心链路。

## 5. 原始日志、恢复与边界

| 进程 | 原始日志 | 正常退出证据 |
|---|---|---|
| PID20566 | [integrated-run.hilog.txt](evidence/leader/2026-11-01/d6-device/integrated-run.hilog.txt) | 20:01:33.426，native main returned 0 |
| PID23172 | [input-restart.hilog.txt](evidence/leader/2026-11-01/d6-device/input-restart.hilog.txt) | 20:15:04.769，native main returned 0 |

两轮 `LEOCAD_GL_RENDERER` 均 passed=true、9 programs、0 skipped、safe_render=false；实际 FBO 完整，depth24/stencil8，保留 CONTEXT / FRAMEBUFFER / READBACK 原始记录。未检出 FATAL、SIGSEGV、GL_INVALID 或 GL_OUT_OF_MEMORY；仍有 Qt/OHOS 窗口、系统服务告警，不能称为“日志零错误”。

结束后应用已正常关闭，所有测试编辑均丢弃。将原 `LeoCAD.conf` 按原字节恢复，再从设备读回：5,420字节，SHA-256 `5575996fecc88dc99bb3513e4d8d5a68da11521b04685dfc3c6c65fa66a13631`，与操作前一致。固定模型读回仍为267字节、上述原哈希。设备保留本轮新正常模式包。

前视适应窗口裁切 #32 沿用组长既有“非阻断接受”决定，没有据此重新打开 issue，也没有声称修复。中文/布局 #28、系统文件选择器与授权读写、GUI保存重开、长期稳定性和上架检查继续按后续阶段推进。本轮没有新增已确认的核心阻断缺陷，也没有创建或关闭远端 issue。
