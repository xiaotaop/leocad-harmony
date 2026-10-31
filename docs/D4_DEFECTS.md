# D4 集成缺陷与后续安排

记录日期：2026-10-30。源码、设备、包哈希和复现边界以 [D4真机报告](D4_DEVICE_ACCEPTANCE.md) 为准。

2026-10-31 后续：下表保留D4当日状态。D5新包的颜色、偏好裁剪复验与新增相机/主题问题见 [D5真机报告](D5_DEVICE_ACCEPTANCE.md) 和 [D5缺陷表](D5_DEFECTS.md)，不要将下表的“待修”误读为最新验证结果。

下表是组长交接和责任安排，已随 [PR #26](https://github.com/xiaotaop/leocad-harmony/pull/26) 推送到 GitHub。新增 [#27 缩略图颜色](https://github.com/xiaotaop/leocad-harmony/issues/27)（A主责、C配合）和 [#28 D7文案与布局](https://github.com/xiaotaop/leocad-harmony/issues/28)（C）；#12/#13/#14/#15 已补充 D4 状态，保留历史记录且继续开放，#16 保持原发布阶段安排。未自动关闭 issue。P0/P1指本轮新观察到的集成问题，不取代发布门槛或许可事项审查。

| 编号 / 级别 | 事实与复现 | 负责人 | 下一步 / 截止阶段 | 状态 |
|---|---|---|---|---|
| D4-01 / P2 | 正常精简库、Brick分类，选择颜色Red，名称和色块为红，缩略图却为蓝；主视图原模型红蓝正确。[截图](evidence/leader/2026-10-30/d4-device/color-red-thumbnail-check.jpeg)，PID48248。LightBlue缩略图偏黄也复现 | A主责、C配合 | D5优先检查离屏回读QImage格式与ARGB32目标逐字节拷贝；修复后对照Red/Blue及插入后的实体颜色，再由组长真机复验 | **待修**；疑似R/B通道顺序，未作为已证明根因 |
| D4-02 / P2，关联#14 | View→选项→常规仍显示检查更新、POV-Ray、LGEO和外部库路径；渲染页仍有Blender设置。[常规](evidence/leader/2026-10-30/d4-device/preferences.jpeg)、[渲染](evidence/leader/2026-10-30/d4-device/preferences-render.jpeg)。与D2资源方案§6.4约定不符；不能把菜单无Import/Export等同于全部设置已裁剪 | C | D5确认裁剪交付时间，采用既有隐藏/禁用控件方案；保留必要显示设置。修复后组长复验 | **待修**，#14继续保留 |
| D4-03 / P2，既有F-02 | Filter Parts、Parts计数、空态、颜色/分类名仍有英文；搜索选项部分文本被省略。[筛选子菜单](evidence/leader/2026-10-30/d4-device/search-filter-options.jpeg)。常用中文主菜单和状态栏无乱码 | C | D7统一翻译新串、检查各宽度的文案和菜单尺寸；不能称本次中文新串已验收 | 已知，继续跟进 |
| #12 / P2 | 旧正常模式网格盖住实体；A #24后，圆圈/线/原点轴开启、两种窗口尺寸、切换视角与重启均未复现；实际FBO完整depth24 | A，组长提供设备证据 | A核对本次报告后决定关闭；与缩略图颜色问题分开跟踪 | **真机复验通过，待负责人确认关闭** |
| #13 / P2 | 本轮“偏好指向不存在的外部库”的准备过程出现旧版库提示；不属于默认qrc正常启动。默认主窗口各正式轮次可正常退出 | 组长 | 后续平台回归中单列缺失自定义路径、模态关闭、Ability是否退出及恢复流程；本轮不扩大为修复完成 | 保留 |
| #15 / D7–D8 | 系统选择器、外部授权/导出、取消/失败及完整GUI建模读写闭环 | 组长+B | 按D7–D8系统集成执行；已有沙箱与核心调用证据只覆盖一部分 | 保留 |
| #16 / 发布准备 | Qt默认权限和剪贴板行为尚未完成发布收敛 | 组长 | 正式签名前按实际使用能力核对；本轮仍用现有调试包权限 | 保留 |

**本轮新P0=0、新P1=0；存在上述P2。** 可以提交D4阶段记录，不能宣称全量质量验收或发布通过。偏好裁剪尚未收口，#14本次不关闭；#12的关闭需A核对。

颜色缺陷定位线索：`lcView::BeginRenderToImage()`建立`Format_ARGB32`目标；`OnDraw()`取`GetRenderFramebufferImage()`后直接`memcpy`；缩略图随后才`convertToFormat(Format_ARGB32)`。建议A验证真机`toImage()`返回格式是否与目标一致。这里是代码线索，尚未在修复版证明，不应只在最终缩略图上盲目交换颜色。

远端对应关系：D4-01 → [#27](https://github.com/xiaotaop/leocad-harmony/issues/27)，D4-02 → [#14](https://github.com/xiaotaop/leocad-harmony/issues/14)，D4-03/F-02 → [#28](https://github.com/xiaotaop/leocad-harmony/issues/28)。#14 标题已更新为偏好设置剩余裁剪，零件库、初版图标和文件菜单的接入不再被描述为未完成。

2026-10-31 PR审核追加：A发现入口日志在 `--version` / `--help` 提前成功退出时解引用空库（P2），已由组长在 `5b79fdd6` 修正为 `loaded_parts:null`，完成真实宿主子进程回归及ARM64编译，并接入CI。此项直接在PR #26修复，未另建重复待办；没有重跑真机或更改原D4证据，详见 [真机报告§9](D4_DEVICE_ACCEPTANCE.md#9-审核修正提前成功退出)。

本次恢复了用户原偏好文件，两个受控坏归档已改名、不再命中资源发现，最终包回到qrc正常库；原始模型与用户数据保留。详细恢复证据和未覆盖范围见真机报告。
