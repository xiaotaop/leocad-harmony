# 成员 B D9：模型、文件与加载生命周期复核

日期：2026-11-04。D9 要求为修复读写、撤销重做、空模型、非法模型和路径问题。交付基线 `25fa7ede6e457ffaa7ea7cf5fdfa8c5be4f5649f`（已合入 C #52）；本次只更新 B 的覆盖矩阵、设备事实与修复包交接，没有新增功能或重复实现。

## 模型与文件覆盖

逐项核对 `tests/model/tst_model_io.cpp` 的现有断言，以下 D9 行为已有覆盖，未发现需要新增 B 生产补丁的缺口。

| D9 范围 | 现有用例及判据 |
|---|---|
| 空模型 | `emptyModelRoundTrip`：合法空工程保存重开后仍为一个模型、零件数为 0；与零字节坏文件区分 |
| LDR / MPD | `ldrRoundTrip`、`mpdRoundTrip`、`sharedSubmodelRoundTrip`：颜色、位姿、子模型引用及共享子模型保持 |
| 撤销重做 | `editingSequenceUndoRedoAndSave`、`insertTwentyPiecesUndoRedoAndReopen`：编辑/历史、保存点、20 件插入及重开 |
| 读取失败 | `missingFilePreservesModel`、`invalidLDrawPreservesCurrentDocument`：不存在、截断、非有限数、重复模型、自引用/循环等失败保留原工程身份、模型与脏状态 |
| 保存失败 | `unwritableDestinationReportsError`、`failedSaveAsPreservesDocumentState`、`streamWriteFailureIsReported`、Windows `failedReplacementPreservesExistingFile`：返回错误，保留文件、工程身份和未保存状态 |
| 路径及覆盖 | `ldrRoundTrip` 的中文带空格路径、`relativeDocumentPathsSurviveWorkingDirectoryChange` 两行、`successfulOverwriteCanBeReopened`：改变工作目录后仍写原绝对路径，覆盖后可重开 |

固定证据：[PR #52 模型 CI 41777449542](https://github.com/xiaotaop/leocad-harmony/actions/runs/41777449542)，head `a0fe499c6e4702829e9e7d69145c1a84439d350a`，产物 `windows-model-3031ef9081be722ad6246ec78bdf5d92419a7ae6`。B 下载并读取原始日志：模型 **65 passed / 0 failed / 0 skipped**、基础渲染 **7/7**、授权保存 **9/9**、菜单 **11/11**；完整应用构建及启动步骤成功。本地化为 **20 passed / 0 failed / 2 skipped**（offscreen 无字体族），未把跳过计作通过。此证据属于 #52，不冒充 #53 同步后的组合验证或本机新跑。

## #45 与 A/C 审核

B 已审核并正常合并 [C #52](https://github.com/xiaotaop/leocad-harmony/pull/52)，合并提交为上述基线。工具栏修复保留 QAction 生命周期，布局恢复后再次隐藏裁空容器；9 项远端检查成功且无未解决讨论。

[A #53](https://github.com/xiaotaop/leocad-harmony/pull/53) 初审 head `18ab23e09eb91eebfbdaed02476c1bfbe1e52cf5`，同步 #52 后为 `c881a97f30fff05754d17894d8a421ba74f3f369`。B 已分别正式批准；同步前后缩略图实现、输入测试与 A 证据目录无变化。复核了以下引用关系：每次共享请求计数加一；未 Loaded 或空 Pixmap 时保留待处理加载引用；成功生成后释放该引用；取消和析构仅释放仍待处理的引用。保留的加载引用保护 `SetPreviewPieceInfo` 替换预览模型时的零件生命周期。

B 独立重算 A 的 **3 项源码、25 项证据**大小/SHA-256，全部一致。Mac 四配置各 **42/0/0**；旧实现的共享用例失败、陈旧通知用例超时。另下载 [Windows 输入 CI 41784535409](https://github.com/xiaotaop/leocad-harmony/actions/runs/41784535409) 原始产物，正常 DPR1、DPR1.5、DPR2、基础 DPR2 各 **42 passed / 0 failed / 0 skipped**。这些结果仅代表初审版本；组合 head 的 CI 和合并状态以 #53 为准。

受控通知测试验证 GUI 不同步等待、当前完成通知随后产生非空缩略图，不能证明 OHOS 自然触发异常加载状态的完整根因。#45 保持 P1 / Alpha 阻断，代码合入不自动关闭。

## D8 设备证据更新

固定来源为组长 [D8 设备报告](https://github.com/xiaotaop/leocad-harmony/blob/07b95434c599135871c507e7840672a163601a73/docs/D8_DEVICE_ACCEPTANCE.md)及其证据目录，属于仍为 Draft 的 #51，不改写其放行状态。被测源码 `d9ff2a4f7b43f23773e5c4fdad91d00ac8674efe`，HAP SHA-256 `d457e954696f56533a731034b1dd5c7fc6f488ae699a713e74ebe0ff423804fa`。

B 重算清单 **64 项文件**大小/SHA-256，全部匹配。核对 `mpd-edited-saved.mpd` 与 `mpd-after-reopen.mpd` 字节相同（SHA-256 `74398c6cf5a05bb7dd9e7ebb11bdce1fd571b98ddd90f591969680183c3f5616`）：2 模型、3 条引用、颜色 4/1/14，红杯 LDraw 位置 `(-45,0,-10)`，子模型引用与内部零件结构保留。

相较 B D8 交付时的待测状态，组长后来已完成新进程 MPD 编辑/撤销重做/保存/退出重开、取消打开/另存为、循环与截断坏文件保护，以及真实关闭 WLAN 后的 LDR 保存重开。以上是复核组长证据，本次 B 未操作设备、未重新构建 HAP。

成功对照不抵消同进程 LDR→MPD 失败。四份有效线程栈属于 PID44779，卡在 `PartLoaded → DrawThumbnail → SetPreviewPieceInfo → LoadPieceInfo`；组长手工 PID59946 复现有日志和截图，但没有新的有效线程栈。两轮证据不能混用。

## 修复包设备交接与剩余项

与 A 的 D9 九步设备请求合并执行，不另开重复批次；采用经审查、全部必需 CI 成功后的 #53 集成源码，组长构建签名新包并记录 commit、HAP 大小/SHA-256 和 native Build ID。B 重点核对：

1. 至少三轮纯手工同进程 LDR→MPD，分别记录面板显示/隐藏、菜单及编辑响应；`LEOCAD_THUMBNAIL_DEFERRED` 后必须最终有真实缩略图，永久空白也算失败。
2. MPD 编辑、撤销重做后另存中文带空格路径，正常退出再经系统选择器重开；读回模型数、引用、颜色、位姿和修改标记。
3. 脏工程下取消打开/另存为、选择坏文件，确认原身份、模型及脏状态保留；合法空模型与零字节坏文件分别验收。
4. 在测试副本上由系统真实拒绝授权或制造可证实的不可写目标，记录失败提示、原文件哈希和未保存状态；不能把未生效的 chmod 当作授权失败。
5. 若发生外部写失败，确认恢复副本用户能取回，原文件未被静默损坏；该授权 QFile 路径仍不保证断电/进程终止时的原子替换。
6. 再无响应时先保留进程、完整 hilog 和至少两次线程快照；记录新 Build ID，与 D8 旧栈分开。测试后恢复配置、原文件并读回校验。

#45 由 A 牵头、B 协查模型/引用，须新包失败序列复验后判断；#15 的实际拒绝授权、无写权限与恢复体验仍开放，由组长执行、B 分析数据结果。上述交接沿用 20:30 前提交、21:00 设备批次的团队安排；未完成项保持开放，不以期限到达代替验收。回退本交付仅还原文档，不影响生产行为。
