# D3 组长集成与真机验收

日期：2026-10-28，提前执行计划中 10/29 的 D3 组长任务。

**D3 出口通过**：完整 LeoCAD 主程序已构建为华为调试签名 HAP，真机显示主窗口，
正常退出并以新进程再次启动。不是最小三角形探针。功能和图形质量仍按后续里程碑验收。

## 固定基线和环境

- 团队 main：`9f9d7b352dfde4c14a75da01c90d42e96b842f6a`，已含 A #9、B #10、C #8。
- 本次代码：`f0dfce8651d7d42c8c37629294d8727107c8ce1e`；后续证据提交不修改业务代码。
- 设备：HUAWEI MateBook Pro / HAD-W32 / ARM64 / API23。
- 设备系统：OpenHarmony 6.1.0.115；Qt 6.12.0；SDK 26.0.0.105；Hvigor 6.26.8。
- GPU：Maleoon 916；OpenGL ES 3.2 B301；GLSL ES 3.20；DPR 约 1.9。
- 签名由 DevEco CLI 官方生成，安装到此前已经授权登记的同一台设备。
- 未提交私钥、证书、Profile、完整设备标识或 HAP。

## 代码接入与启动问题

1. Harmony 入口明确请求 GLES 3.0、24 位深度与 8 位模板；保留全局共享上下文。
2. Harmony 将仓库现有 `:/resources/library.zip`（128 个顶层件）登记为启动库。
   这是 D3 集成资源，不代表 C 的 1160 件精简库已接入或验收。
3. Harmony 链接 Qt Svg，使 SVG 图标插件随包部署。
4. 增加 `scripts/build-harmony-app.ps1`，复用已验证的 SDK/CLI 环境隔离与恢复。
5. 增加默认关闭的 `LEOCAD_BUILD_MODEL_TEST_HAP`，独立测试包不进入主程序。

首次直接运行 main 时，缺库提示成为第一个原生窗口；关闭提示后，主窗口创建报
`can't create window without the default Ability instance` 并 SIGABRT。
显式接入现有内置库后正常启动不再经过此提示，最终两轮均干净退出。
异常启动参数可能触发同类首窗口模态问题，见后续问题表，不能据此声称所有错误路径已修复。

## 主程序包与两轮结果

- Bundle：`io.github.xiaotaop.leocad`；Ability：`QAbility`；版本 `0.1.0` / code 1。
- 签名包 SHA-256：`2c25af3d775ca1c17805aa0333aacc7c04c0cc7c49704f1a204f379626160bd7`。
- 本机保留：`build/harmony-d3-evidence/leocad-f0dfce86-signed.hap`。
- 第一次独立进程 PID 44809：主窗口、零件缩略图、颜色面板正常；通过菜单触发 3DS
  导出立即提示 `This format is disabled in this build.`；关闭提示和主窗口后 native main 返回 0。
- 第二次独立进程 PID 45162：通过 Qt Want 参数加载固定 `render-baseline.ldr`，两块红/蓝
  积木可见；放大、恢复窗口后仍重绘；关闭主窗口后 native main 返回 0。
- 未用 `force-stop` 代替这两轮正常退出。前期错误传送路径调试曾使用 force-stop，
  不计入正式通过轮次。

两轮 `renderer_offscreen` 均满足：`passed=true`、`error` 为空、`gles_build=true`、
`is_gles=true`、`is_current=true`、`share_requested=true`、`sharing=true`；
`surface_requested_format` 与 `context_format` 相等，实际 GLES 3.2、depth24/stencil8。
主视图 `view_initialize` 也报告有效、current、共享成功。

截图：[主窗口](evidence/leader/2026-10-28/d3-device/run-1.jpeg)、
[固定模型与放大窗口](evidence/leader/2026-10-28/d3-device/run-2.jpeg)、
[恢复窗口](evidence/leader/2026-10-28/d3-device/restored.jpeg)、
[格式禁用提示](evidence/leader/2026-10-28/d3-device/format-blocked.jpeg)。

## A、B 的 D2 待真机项

| 项目 | 本次结论 |
|---|---|
| A：主程序 ARM64/HAP、上下文与 Surface、GLES 版本、共享关系 | 已在生产主程序验证，见两轮日志 |
| A：固定模型、窗口变化、退出重启 | 已执行；模型可见，但网格覆盖模型的现象仍需处理，不声明渲染质量全部通过 |
| B：OHOS 两个隔离宏 | 主程序和测试目标的真实编译命令均含 `LC_DISABLE_NETWORK=1`、`LC_DISABLE_NON_CORE_FORMATS=1` |
| B：模型、历史、LDR/MPD、坏输入及格式禁用 API | 同设备独立 ARM64 测试包 50 passed / 0 failed / 0 skipped |
| B：主程序可达非核心导出入口 | 3DS 菜单实际触发并显示禁用提示，未出现文件选择/写入流程 |
| B：物理断网下的完整鼠标建模、系统文件选择器和外部导出 | 未做整套人工验收；后续联调保留，不用 API 测试替代 |

测试包：`io.github.xiaotaop.leocad.modeltests`，SHA-256
`53dd2093fb60528552eb5220fdc182f6173c4f555437ce6bcee75d580800b20a`。
本机保留 `build/harmony-d3-evidence/modeltests-f0dfce86-signed.hap`。

原测试断言未放宽。为使桌面测试在设备上运行，仅将相同的三个样例内嵌测试包，
再写为沙箱中的可写副本，输出测试报告到 `.qttest/model-tests.txt`。
初轮直接从 qrc 复制会保留只读权限，导致两项覆盖保存测试失败；改用可写副本后重跑全部通过。
Windows 51 项与设备 50 项的差异来自 `#ifdef Q_OS_WIN` 下的 Win32 文件锁测试，
不是通过跳过失败用例取得通过。

**CLI 注意**：`devecocli run` 以进程持续存活判断启动冒烟，会把短时 QtTest 的正常
退出报作 `FAIL_CRASH`。本测试包的最终判据是原始报告 50/0/0、
`LEOCAD_MODEL_TESTS ... exit 0` 和同 PID 43199 的 native main 返回 0，三者一致。

## 问题发现来源与计划执行阶段

这五项均在本次 D3 集成验收中记录，**发现阶段不等于计划完成阶段**。
旧编号中的 `D3-` 仅表示发现来源，现以远端 Issue 号和下表的执行阶段为准。
它们不是组长 D3 的新增验收条件；组长“主窗口启动、正常退出、二次启动”的出口已经通过。

| Issue | 计划执行阶段 | 级别/负责人 | 现象与交付范围 |
|---|---|---|---|
| #12 | D4–D5（10/30–10/31）图形联调 | P2 / A | 网格圆圈和线条覆盖模型；分析深度与绘制顺序并复验。两块模型可见，但尚非图形质量放行 |
| #13 | 后续平台回归，具体批次待排 | P2 / 组长，必要时与 Qt 平台协作 | 正常默认启动已避开首窗口模态问题；继续专项复现异常文件/参数路径，不要求 D3 当天关闭 |
| #14 | 成员 C 的 D3（10/29）桌面接入；真机随集成批次复验 | C | 菜单裁剪、精简零件库与初版图标的桌面加载；正式图标冻结仍按发布计划执行，不阻塞组长已通过的启动验收 |
| #15 | D7–D8（11/2–11/3）系统选择器与完整闭环 | 组长+B | 前期编辑/文件工作可按原计划并行准备；最终验收包含导入导出、取消/权限和真实用户路径的完整 UI 闭环 |
| #16 | 发布准备阶段，正式签名前完成 | 组长 | 按首发功能收敛 Qt 模板权限，核对剪贴板拒绝路径及发布材料一致性 |

按组长后续要求，以上事项已建立远端跟踪：
[网格绘制 #12](https://github.com/xiaotaop/leocad-harmony/issues/12)、
[Ability 生命周期 #13](https://github.com/xiaotaop/leocad-harmony/issues/13)、
[界面与资源 #14](https://github.com/xiaotaop/leocad-harmony/issues/14)、
[文件闭环 #15](https://github.com/xiaotaop/leocad-harmony/issues/15)、
[权限收敛 #16](https://github.com/xiaotaop/leocad-harmony/issues/16)。
Issue 已按上表分派负责人，并链接固定提交上的证据。

## 复现与证据

主程序：

```powershell
./scripts/build-harmony-app.ps1 -SdkRoot '<CLT/sdk>' `
  -QtRoot '<Qt/6.12.0/harmonyos_arm64_v8a>' -GenerateSignature -Run -Device '<HDC目标>'
```

测试包：

```powershell
./scripts/build-harmony-app.ps1 -SdkRoot '<CLT/sdk>' `
  -QtRoot '<Qt/6.12.0/harmonyos_arm64_v8a>' -Target ModelTests -GenerateSignature
hdc install -r '<测试包 entry-default-signed.hap>'
hdc shell aa start -b io.github.xiaotaop.leocad.modeltests -a QAbility
hdc file recv -b io.github.xiaotaop.leocad.modeltests `
  /data/storage/el2/base/files/.qttest/model-tests.txt '<本机输出>'
```

Qt 6.12 的 `io.qt.appArgsJson` Want 参数可在冷启动时传入模型路径。
HDC 沙箱传送相对源路径可能保留 `tests/fixtures/` 子路径，使用前应回收文件并核对路径和哈希。
本次固定模型实际位于 `/data/storage/el2/base/files/tests/fixtures/render-baseline.ldr`，
不代表系统文件选择器已经接通。

原始证据与 SHA-256 收据位于
[d3-device](evidence/leader/2026-10-28/d3-device/receipt.json)。
10/27 的 D2 通过证据与签名包未修改。代码与证据已推送至
[PR #11](https://github.com/xiaotaop/leocad-harmony/pull/11)，按组长当前要求不请求审核人。
