# 成员 A D3：正常与基础渲染真机验收

日期：2026-10-29，约 22:40–22:46。**核心设备冒烟通过，未声明所有材质/效果用例全部通过。**

## 固定源码与环境

- A 的代码：`0fe71124f3d6af6b0ee7e0fc6ff9e7f89dc29597`，已由 PR #20 合入 `main@25940722`。
- 本次构建源码：`cedabd92902ceb5c9208eb63a9e6acddca8cade1`，将该 main 与组长图标/证据分支合并；没有修改 A 的业务代码。该集成源码同时保留 PR #21 的图标配置。
- HAD-W32 / ARM64 / API23，Qt6.12.0，SDK26.0.0.105，Maleoon916 / GLES3.2 B301 / GLSL ES3.20，DPR约1.9。
- 使用已验证官方调试签名和同一 Bundle `io.github.xiaotaop.leocad`，两个包交替覆盖安装，保留沙箱和用户偏好；不清除用户数据。
- 输入是原样 `tests/fixtures/render-baseline.ldr`，证据目录保留同字节副本。通过 Want 参数加载沙箱 `/data/storage/el2/base/files/a-d3-device.ldr`；不是系统选择器验收。

## 双包构建

| 项目 | 正常包 | 基础包 |
|---|---|---|
| 构建目录 | `build/harmony-d3` | `build/harmony-a-d3-basic` |
| CMake 开关 | 显式 `LEOCAD_SAFE_RENDER=OFF` | 显式 `LEOCAD_SAFE_RENDER=ON` |
| 编译命令 | 82条，0条含 `LC_SAFE_RENDER=1` | 82条，全部含 `LC_SAFE_RENDER=1` |
| 包字节数 | 80,076,023 | 80,036,846 |
| HAP SHA-256 | `a88393354ee7c4e7da2ac17c6c24090c1a68738a633cfdd9c3f43df012002747` | `5d286143bd2f58652f53381d57ae9eab4404eef5e7f5c1ae90ceabdfbe9e2598` |

正常包复用原有正常构建目录并显式配置 OFF；基础包使用独立新目录，未在同一缓存中切换两种模式。真实编译命令和运行日志均验证开关，不依赖 HDC shell 环境变量传入 Ability。两包均通过现有 `scripts/build-harmony-app.ps1 -GenerateSignature` 构建签名；Qt模板的既有ArkTS弃用警告保留。

本地固定副本：`build/harmony-d3-evidence/a-d3-cedabd92-{normal,basic}-signed.hap`。不提交HAP、证书或完整设备标识。

## 五轮运行结果

| 轮次 | PID | renderer passed | safe_render | programs / skipped | native main |
|---|---:|---|---|---|---|
| 正常首次 | 10128 | true | false | 9 / 0 | 返回0 |
| 正常重启 | 10658 | true | false | 9 / 0 | 返回0 |
| 基础首次 | 11204 | true | true | 6 / 3 | 返回0 |
| 基础重启 | 11382 | true | true | 6 / 3 | 返回0 |
| 装回正常包 | 12261 | true | false | 9 / 0 | 返回0 |

每轮原始 hilog 均保留 `LEOCAD_GL_RENDERER`、`LEOCAD_GL_CONTEXT` 和对应 native main 退出行。每轮上下文有效、current、共享成立；使用主窗口关闭按钮正常退出，没有用强停作为通过判据。最后已恢复 OFF 正常包并关闭应用。

## 视觉与交互

- 正常包两砖实体、颜色、字体和零件缩略图可见；点击红砖有选择着色/边框和操纵器；ViewCube切换前视图有效；最大化与恢复窗口后继续重绘。
- 基础包同一模型显示红/蓝实体与普通边线，额外光照关闭后呈平面着色，无网格；选择反馈和字体仍可用。方向前视与 Home 视角切换、窗口最大化/恢复及再次启动均可用。
- 同视角对照使用 `normal-restart.jpeg` 和 `basic-max.jpeg`（同一前视图），能看到正常包网格存在、基础包网格消失。其他截图用于窗口和相机操作，各自视点或缩放可能不同，不能用于像素一致性比较。
- `normal-restored.jpeg` 采在窗口动画中，稳定恢复结果以 `normal-restored-stable.jpeg` 为准。
- 正常模式网格仍覆盖积木表面；恢复正常包后再次出现。基础模式隐藏网格是显式降级，**不关闭 Issue #12，不声明深度/遮挡根因修复**。

## 未覆盖与准备过程

- 固定两砖没有真实透明材质或专门纹理样例：真实透明度和基础贴图暂未验，不能拿半透明选择着色当透明材质通过。条件线、步骤淡化/高亮也未用专项样例逐项目视确认；6/3日志证明走了基础模式，不替代这些视觉验收。
- 没有在验收HAP中故意破坏shader；失败传播负例沿用A的宿主测试记录，本次仅验证实际设备正常shader初始化路径。
- 准备阶段首次沙箱传输报 `Invalid bundle name`，导致首次启动指向了未正确传入的路径并显示文件错误提示；纠正传输、换用准确文件路径并重新启动后，才开始上述正式轮次。该准备过程不计入五轮通过，记录保存在 `setup-error.hilog.txt` / `setup-error.jpeg`。
- 不代表长期稳定性、系统选择器、外部导出或发布验收全部通过。

证据：[a-d3-render](evidence/leader/2026-10-29/a-d3-render/)。`builds.json` 固定编译模式和两包哈希，`reports.json` 从原始日志抽取上下文/渲染报告，`receipt.json` 固定所有证据文件的SHA-256。
