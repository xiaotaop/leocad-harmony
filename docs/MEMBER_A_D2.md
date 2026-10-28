# 成员 A：2026-10-28（D2）交付

## 结论

已完成今天的探针结果核对、主程序上下文日志和首批 `lc_context` 兼容修改。
新增共享上下文回归测试，本机 Qt Test 13/13 通过，完整应用构建、模型导出及
实际视图初始化日志通过。组长既有的最小 GL 探针真机结果已核验；
**本次生产补丁尚未进行 HarmonyOS 打包或真机运行**。

## 远程仓库检查

检查时 main 为 `32ce8425f5fa7f14b7a4a837ead2339a314a66e8`，tree 为
`a05d89b974fe9a1246f3c5f1fe131339240d12fa`。已验证下载归档的完整 tree 和原始
签名 commit，且保留只读用途的 `upstream` 远程。

- PR #3（A D1）、#4（组长 D1）、#5（B 第二批）、#6（C D1 文档）已进入 main。
- [PR #7](https://github.com/xiaotaop/leocad-harmony/pull/7) 为组长的 D2 最小 HAP、
  真机证据和脚本修正；核对版本 `42d00d83c701d117b2d318d771701ca8b17c308f`，检查时尚未合入。
- PR #1 是 C 的完整 UI/零件库工作，仍待审核；本次不叠加该分支。
- 本次只改 A 的上下文/视图初始化，没有修改 PR #7 正在维护的 `tests/render/`。

## 真机探针结果核对

原始入口：[组长 D2 真机验收](https://github.com/xiaotaop/leocad-harmony/blob/42d00d83c701d117b2d318d771701ca8b17c308f/docs/D2_DEVICE_ACCEPTANCE.md)。
探针源码 `ebb767bc39bd2c66602a4c5ebfec234d28b20c38`，设备运行时间为 **10/27**；
A 在 10/28 核验证据，没有把它写成今天重新运行了自己的补丁。

| 项目 | 已归档的设备结果 |
|---|---|
| 设备/系统 | MateBook Pro / HAD-W32 / API23；OpenHarmony 6.1.0.115 |
| 图形 | HUAWEI Maleoon 916；OpenGL ES 3.2 B301；GLSL ES 3.20 |
| Qt/平台 | 6.12.0 / ohos / arm64 |
| 两轮进程 | PID 20760、20872，原生 main 均返回 0，JSON 均为 passed=true |
| 材质 | 九组编译和链接均通过 |
| 资源 | VBO、深度/stencil FBO 读回、共享 context/纹理通过 |
| 窗口 | 800×500，DPR≈1.9，framebuffer 1520×950 |

A 独立核对了每轮 JSON/PNG 的 SHA-256，确认同 PID 日志中的完整 JSON 与原始文件
一致，且同 PID 退出记录为 0。结果见
[leader-evidence-check.json](evidence/member-a/2026-10-28/leader-evidence-check.json)。
不复制或重写原始设备证据，也不宣称完整 LeoCAD 已通过真机验收。

## 代码改动

| 文件 | 作用 |
|---|---|
| `common/lc_glcontext.h` | 可直接测试的上下文创建与诊断 helper；Surface 使用 context 实际 format/screen，验证共享组与 GUI 线程 |
| `common/lc_context.cpp` | 接入 helper；在资源分配前拒绝 GL/GLES 构建路径不一致或 GLES 低于 3；输出成功/失败 JSON 日志 |
| `common/lc_viewwidget.cpp` | 每次 initializeGL 输出视图实际格式、共享组、current 状态、DPR 和尺寸 |
| `tests/render-context/` | 直接测试生产 helper；独立于已有能力探针及主程序构建 |
| `.github/workflows/render-context-ci.yml` | Qt 6.12 + Mesa/Xvfb 的共享资源回归，结果随本次 PR 提供 |

正常成功后仍由 UI 线程持有全局离屏上下文。没有 share context 时保留上游的
非共享离屏使用方式；存在 share context 时必须确认共享组成立。
helper 不在后台线程创建 Surface，不在非 current context 上查询 GL。

API 检查只做明确失败保护，不把桌面 GLSL 110 自动改为 GLES 源码，也不假定
关闭 shaders 就能在 GLES 2 上运行。目标主程序若报 API/版本不符，组长应核对
Kit 宏与应用入口在 QApplication 前请求的 GLES 格式，再进入下一真机批次。

## 本机验证

环境：macOS 27.0.1 / ARM64 / Apple M4 / Qt 6.12.0 / Apple Clang 21 / CMake 3.28.2。

- 独立上下文测试：11 个用例 + init/cleanup，合计 **13 passed / 0 failed / 0 skipped**。
- 共享纹理通过独立 FBO 读回四个 RGBA 通道；预先设入错误格式后仍正确继承父上下文。
- 覆盖无效父上下文、重复创建、非 GUI 线程和非 current 日志安全路径。
- 完整主程序 Release 构建通过，固定两砖模型导出 800×600 PNG。
- 主窗口启动冒烟实际触发两个视图 initializeGL；两者 context 均 valid/current/sharing，DPR=2。
- 主窗口冒烟使用独立测试进程，五秒后终止该进程；它不证明干净退出、休眠恢复或长期稳定性。
- 图形日志的 `renderer_offscreen` 为 passed=true，format/screen 共享约束成立。

新增 CI 首轮因精简 Qt 安装未包含 ICU 73 而在链接阶段失败（运行 `40641456459`）。
已核对官方 Qt 6.12 Linux 归档并将安装项修正为 `qtbase icu`；这是 CI 运行库补齐，
不修改已通过宿主验证的生产源码。最终 CI 结果以本次 PR 的最新提交为准。

完整构建使用 `/tmp` 下的 ASCII 路径干净快照（固定 main + 三个生产文件改动）。
本机在含括号的当前工作区运行根 CMake 时，Qt Linguist 的延迟参数解析失败并生成了
错误的翻译路径；已清理此次生成文件，未修改根构建或上游翻译。独立 context 测试
可以直接在当前工作区构建。桌面 SDK 补齐 qtsvg 后再采集最终窗口日志。

### 复现

上下文测试见 [README](../tests/render-context/README.md)。主程序在 ASCII 源路径下：

```sh
cmake -S /tmp/leocad-source -B /tmp/leocad-build \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.12.0/macos -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/leocad-build --parallel 4
/tmp/leocad-build/leocad /tmp/leocad-source/tests/fixtures/render-baseline.ldr \
  -l /tmp/leocad-source/resources/library.zip --image /tmp/model.png --width 800 --height 600
```

## 证据和后续

- [构建/哈希记录](evidence/member-a/2026-10-28/build-record.json)
- [Qt Test 文本](evidence/member-a/2026-10-28/context-tests.txt)、[JUnit XML](evidence/member-a/2026-10-28/context-tests.xml)
- [主程序上下文 JSON](evidence/member-a/2026-10-28/window-contexts.json)
- [固定模型图](evidence/member-a/2026-10-28/macos-model.png)
- [真机交接步骤与回退](MEMBER_A_D2_DEVICE_HANDOFF.md)

GL-01/02 已提交首轮兼容实现并完成宿主验证；GL-03 增加明确的 API/版本检查。
它们的目标设备验证仍待组长执行。GL-04 的材质链接失败传播、GL-05 的资源重建、
输入/拾取、高分屏定位和高级效果降级继续按 D3 以后计划处理。

本 PR 由成员 B 审核，组长合入。回退详见交接文档，不自动合并。
