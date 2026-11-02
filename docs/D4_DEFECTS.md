# D4 集成缺陷与后续安排

记录日期：2026-10-30。源码、设备、包哈希和复现边界以 [D4真机报告](D4_DEVICE_ACCEPTANCE.md) 为准。

2026-10-31 后续：下表保留D4当日状态。D5新包的颜色、偏好裁剪复验与新增相机/主题问题见 [D5真机报告](D5_DEVICE_ACCEPTANCE.md) 和 [D5缺陷表](D5_DEFECTS.md)，不要将下表的“待修”误读为最新验证结果。

下表是组长交接和责任安排，已随 [PR #26](https://github.com/xiaotaop/leocad-harmony/pull/26) 推送到 GitHub。新增 [#27 缩略图颜色](https://github.com/xiaotaop/leocad-harmony/issues/27)（A主责、C配合）和 [#28 D7文案与布局](https://github.com/xiaotaop/leocad-harmony/issues/28)（C）；#12/#13/#14/#15 已补充 D4 状态，保留历史记录且继续开放，#16 保持原发布阶段安排。未自动关闭 issue。P0/P1指本轮新观察到的集成问题，不取代发布门槛或许可事项审查。

| 编号 / 级别 | 事实与复现 | 负责人 | 下一步 / 截止阶段 | 状态 |
|---|---|---|---|---|
| D4-01 / P2 | 正常精简库、Brick分类，选择颜色Red，名称和色块为红，缩略图却为蓝；主视图原模型红蓝正确。[截图](evidence/leader/2026-10-30/d4-device/color-red-thumbnail-check.jpeg)，PID48248。LightBlue缩略图偏黄也复现 | A主责、C配合 | D5优先检查离屏回读QImage格式与ARGB32目标逐字节拷贝；修复后对照Red/Blue及插入后的实体颜色，再由组长真机复验 | **待修**；疑似R/B通道顺序，未作为已证明根因 |
| D4-02 / P2，关联#14 | View→选项→常规仍显示检查更新、POV-Ray、LGEO和外部库路径；渲染页仍有Blender设置。[常规](evidence/leader/2026-10-30/d4-device/preferences.jpeg)、[渲染](evidence/leader/2026-10-30/d4-device/preferences-render.jpeg)。与D2资源方案§6.4约定不符；不能把菜单无Import/Export等同于全部设置已裁剪 | C | D5确认裁剪交付时间，采用既有隐藏/禁用控件方案；保留必要显示设置。修复后组长复验 | **待修**，#14继续保留 |
| D4-03 / P2，既有F-02 | Filter Parts、Parts计数、空态、颜色/分类名仍有英文；搜索选项部分文本被省略。[筛选子菜单](evidence/leader/2026-10-30/d4-device/search-filter-options.jpeg)。常用中文主菜单和状态栏无乱码 | C | D7统一翻译新串、检查各宽度的文案和菜单尺寸；不能称本次中文新串已验收 | **已修待真机复验**：D7 显示层修复见 [#41](https://github.com/xiaotaop/leocad-harmony/pull/41)（已合并）、字体与常用提示见 [#43](https://github.com/xiaotaop/leocad-harmony/pull/43)，详见下文两节 |
| #12 / P2 | 旧正常模式网格盖住实体；A #24后，圆圈/线/原点轴开启、两种窗口尺寸、切换视角与重启均未复现；实际FBO完整depth24 | A，组长提供设备证据 | A核对本次报告后决定关闭；与缩略图颜色问题分开跟踪 | **真机复验通过，待负责人确认关闭** |
| #13 / P2 | 本轮“偏好指向不存在的外部库”的准备过程出现旧版库提示；不属于默认qrc正常启动。默认主窗口各正式轮次可正常退出 | 组长 | 后续平台回归中单列缺失自定义路径、模态关闭、Ability是否退出及恢复流程；本轮不扩大为修复完成 | 保留 |
| #15 / D7–D8 | 系统选择器、外部授权/导出、取消/失败及完整GUI建模读写闭环 | 组长+B | 按D7–D8系统集成执行；已有沙箱与核心调用证据只覆盖一部分 | 保留 |
| #16 / 发布准备 | Qt默认权限和剪贴板行为尚未完成发布收敛 | 组长 | 正式签名前按实际使用能力核对；本轮仍用现有调试包权限 | 保留 |

**本轮新P0=0、新P1=0；存在上述P2。** 可以提交D4阶段记录，不能宣称全量质量验收或发布通过。偏好裁剪尚未收口，#14本次不关闭；#12的关闭需A核对。

颜色缺陷定位线索：`lcView::BeginRenderToImage()`建立`Format_ARGB32`目标；`OnDraw()`取`GetRenderFramebufferImage()`后直接`memcpy`；缩略图随后才`convertToFormat(Format_ARGB32)`。建议A验证真机`toImage()`返回格式是否与目标一致。这里是代码线索，尚未在修复版证明，不应只在最终缩略图上盲目交换颜色。

远端对应关系：D4-01 → [#27](https://github.com/xiaotaop/leocad-harmony/issues/27)，D4-02 → [#14](https://github.com/xiaotaop/leocad-harmony/issues/14)，D4-03/F-02 → [#28](https://github.com/xiaotaop/leocad-harmony/issues/28)。#14 标题已更新为偏好设置剩余裁剪，零件库、初版图标和文件菜单的接入不再被描述为未完成。

2026-10-31 PR审核追加：A发现入口日志在 `--version` / `--help` 提前成功退出时解引用空库（P2），已由组长在 `5b79fdd6` 修正为 `loaded_parts:null`，完成真实宿主子进程回归及ARM64编译，并接入CI。此项直接在PR #26修复，未另建重复待办；没有重跑真机或更改原D4证据，详见 [真机报告§9](D4_DEVICE_ACCEPTANCE.md#9-审核修正提前成功退出)。

本次恢复了用户原偏好文件，两个受控坏归档已改名、不再命中资源发现，最终包回到qrc正常库；原始模型与用户数据保留。详细恢复证据和未覆盖范围见真机报告。

## D7 修复记录：D4-03 / F-02（显示层）

2026-11-02，C。PR [#41](https://github.com/xiaotaop/leocad-harmony/pull/41)。组长报告的三条逐一对应：

**英文残留。** `Filter Parts`、`Parts: %1`、两种空态、缺件横幅、`Icons`/`Filter`/`Lock Color`/`Part Description`/`Regular Expression`/`Open Submodel` 共 11 条新串进入 `resources/leocad_zh_CN.ts`。`lrelease` 结果 1883 finished / 0 unfinished（含后续两批，见下）。

**分类与颜色名。** 分类名和颜色名是**数据**不是文案：分类名是 profile 往返的键，颜色名是会写进模型和导出文件的 `char[64]`。原地翻译会让一种语言下保存的文件在另一种语言下读不出来，译文超过 64 字节还会截断。因此新增 `lcGetCategoryDisplayName()` / `lcGetColorDisplayName()`，只在绘制时查翻译；`lcGetColorToolTip()` 走同一函数。新增 30 个分类名（`Categories` context）和 337 个颜色名（`Colors` context）。

**菜单省略。** 不是缺翻译，是尺寸顺序问题：选项菜单在 `aboutToShow` 里清空重建，而 Qt 在那之前已算好宽度，宽度来自上一次内容——这才是「较长的项被中间省略、旁边稍短的项正常」的原因。按实测文本设最小宽度有效；`adjustSize()`、预填充、`sizeFromContents()` 派生宽度均无效，子菜单必须各自调用。本机对照：主菜单 221→259（需 259），Filter 子菜单 100→205（需 205）。

**后续两批（PR [#43](https://github.com/xiaotaop/leocad-harmony/pull/43)）。** #41 只覆盖了显示层文案；
D7 的 C 行还要求「字体」和「常用提示」，这两项当时没有任何记录，而「字串已在 `.ts` 里」推不出
「用户能看见」。补上：

- **字体**：缺字形时屏幕上是空白或方框，和设备报告的「仍是英文」在截图里长得一样。初版
  `chineseGlyphsAreAvailableToTheUiFont()` 用 `QFontMetrics::inFont()` 逐字查字形覆盖，
  **在 Windows CI 上失败**（DirectWrite 的 `inFont` 只答 family 自身，Segoe UI 对所有中文答否，
  尽管绘制照样回退；Linux 的 fontconfig 把回退并进字体所以本地全绿）。逐字覆盖随引擎语义变化，
  不是可跨主机断言的产品属性——改为只断言平台无关的部分（`.ts` 完整可读、行高 > 0、
  中文测量宽度大于拉丁）。第二版又因 `ascent/descent > 0` 在 Windows 红一次：DirectWrite 对雅黑
  报排版语义（typo descent=0），fontconfig 报上下文语义（>0），两边都没错，问的不是同一个问题；
  连同 `height >= a+d` 一并移除——它通过靠的是排版度量恒等式的构造，不是引擎承诺。规则写进注释：
  **跨引擎只断言语义无歧义的量**，逐字字形覆盖归 `tools/d7/tst_d7_fonts.cpp` 探针在**真机**执行。另加
  `chineseTextIsNotWiderThanTheSpaceItsEnglishPredecessorHad()`——只断言「现在放得下」的话，
  翻译变长时宽度跟着变、测试照样通过，反而掩盖真正要看的事；这条对具体字体不敏感（CJK 全角单字
  ≈1em 且同义中文串字数减半），宿主实测 `筛选零件` 48 px 对 `Filter Parts` 63 px。
- **常用提示**：17 条打开/保存失败的 `QMessageBox` 正文全部翻译，并单独检查格式串没丢
  `%1`/`%2`。这些是用户正常操作就会撞见的（打开坏文件、另存为不支持的格式）。

负向验证时有一条**没能失败**：把字体族换成不存在的名字，Qt 会逐字回退、照样画出中文，验证不出
问题。改用任何 CJK 字体都没有字形的字符（U+12000）才真正触发，报出
`2 characters have no glyph in "DejaVu Sans": 𒀀`。这个弯路记在
`docs/D7_中文资源与菜单尺寸.md` §5.1——**负向验证本身也可能是无效的**。

顺带修了一个 `QVERIFY2`：它的消息参数总会求值，里面调 `.first()` 在成功路径上对空列表断言，
导致通过的测试 abort。

**验证。** `tests/ui/d7`（22 项）驱动真实面板与真实菜单处理器，已做过负向验证：注释掉宽度修复后报 `menu width 0 does not fit` 并失败，恢复后通过。宿主截图在 `docs/evidence/member-c/2026-11-02/d7-host/`，两种窗口尺寸（1280×800 / 1920×1080）各一套。全量回归 9 套件全绿（archive 13、priority 8、menu 8、modellist 5、panel 15、preferences 9、theme 14、chain 9、localization 22，共 103 项）。

**顺带发现并移交的 P0（不属 D4-03，但由本轮核查带出）。** 核查隐私声明「不申请网络权限」时发现：
启动时的更新检查在 `#if !LC_DISABLE_UPDATE_CHECK` 之后，而该宏只在 `macx` 块无条件定义，
鸿蒙不在其中；`CheckUpdates` 默认值 1、首次启动 `LastCheck` 为空，**因此第一次启动就会访问
`leocad.org/updates.txt`**。详见 `docs/release/已知问题.md` §5.1，已由
`scripts/library/test_network_scope.py` 静态锁定并接入 CI。**鸿蒙构建是否传了 `DISABLE_UPDATE_CHECK=1`
无法从本仓库确认**（`platform/harmonyos/` 只有说明文档），需组长在真机确认——这是 C 侧交不出的
一环，不能算作已解决。

**边界，如实记录：**

- 上述截图是 x86_64 **宿主**截图（WSLg Wayland + `QWidget::grab()`），不是真机截图。真机复验仍由组长按计划执行，**本次不宣称已通过真机验收**。
- 三类问题不随本次修复关闭：D4-01 缩略图颜色（[#27](https://github.com/xiaotaop/leocad-harmony/issues/27)，A主责/C配合）、偏好设置裁剪（[#14](https://github.com/xiaotaop/leocad-harmony/issues/14)）、系统文件选择闭环（#15）。用翻译修复去替代缩略图问题的做法不可接受。
- 尚有 75 条英文串未译（`origin/main` 时 97 条，本次修好 22 条，无新增缺口）。分两类：
  - **51 条是命令行 `--help` 的参数说明**（以及只有命令行路径会打印的 `StdOut`/`StdErr` 消息，如 `Saved '%1'.`）。鸿蒙首发版没有命令行入口，这些串到不了用户面前。
  - **24 条是相机、灯光的专业参数**（`Cone Angle`、`Penumbra Angle`、`Power of the light in Watts` 等）和属性编辑字段。这些需要先定术语表才能翻，不能逐词直译；其中 `POV-Ray Export`/`Blender Settings` 属于 #14 的裁剪范围，翻译等于给即将移除的控件做中文。
  两类都不在 F-02（零件面板）范围内，单列跟进。
- **Windows CI 日志失明（本轮连带修复，不属 D4-03）**：`Run-D3Tests.ps1` 用 `*>` 重定向原生进程，
  runner 上落盘恒为 0 字节，9 个套件「成功」从不打印任何 PASS——判定只剩退出码，空日志里永远搜不到
  `FAIL!`。已改用 QtTest 自带 `-o file,txt`（`Build-MemberB.ps1` 一直用这个方式且日志非空），并把
  退出码打回 console。详见 `docs/D7_中文资源与菜单尺寸.md` §5.1b。此前的 Windows「绿」只有退出码
  作证据，日志作证据的从下一次运行开始。
