# 成员 A：2026-10-27（D1）交付记录

## 当日结论

完成 D1 三项工作：桌面编译基线、GL/FBO/共享上下文/着色器风险梳理、独立能力探针。
本次没有修改任何 `common/` 或 `qt/` 业务源码，不提前宣称鸿蒙兼容补丁已经完成。
目标 HarmonyOS PC、GLES 及 HAP 集成仍待组长真机验证。

## 远程仓库检查

- 团队仓库：<https://github.com/xiaotaop/leocad-harmony>。
- 本次锁定 main：`3e2e15b5bcd6ba87720bd2dfebffd49fd48ee3c0`，tree：
  `9e1e8992b398ffcf4015c9cf08304d8c33f7004c`。
- main 已合入 B 的 PR #2（安全保存、输入保护和历史测试）；C 的 PR #1 尚未合入。
  A 基于最新 main 开发，没有叠加 C 分支，也没有修改其界面/资源。
- 远程 `feature/a-render` 初始指向 `959fb4eee3e020e2579fdf7b4196c9856160f759`，
  比 main 落后七个提交且无独立改动；本次在 main 上建立 A 的交付。
- 本地保留 `upstream=https://github.com/leozide/leocad.git`。
- Git 智能 HTTP 下载超时，改用 GitHub API 的源码归档；已重建并校验原始签名
  commit 和完整 tree，二者 SHA 与远程一致，不是另建一个无历史关联的快照提交。

## 桌面基线与结果

| 项目 | 实测 |
|---|---|
| 主机 | macOS 27.0.1 / ARM64 / Apple M4 |
| Qt | 官方桌面 SDK 6.12.0（qtbase、qttools、qtdeclarative） |
| 编译器 | Apple Clang 21.0.0，Command Line Tools，macOS SDK 27.0 |
| CMake | 3.28.2，仅用于独立探针；主程序保持上游 qmake 构建 |
| 应用构建 | Release / ARM64，编译及链接成功 |
| 应用冒烟 | 打开固定两砖模型，使用包内库，导出 800×600 PNG，退出码 0 |
| 探针 | 构建脚本从 configure/build 到运行通过，退出码 0 |
| GL / GLSL | `2.1 Metal - 91.7` / `1.20`，不是 GLES |
| 材质着色器 | 全部 9 组编译/链接通过 |
| 缓冲与 FBO | VBO 上传、含深度/模板附件的 FBO 及颜色读回通过 |
| 共享资源 | UI 线程共享 context/surface 创建、makeCurrent、纹理对象可见性通过 |
| 窗口与缩放 | DPR=2 及环境模拟 DPR=1.25 均通过像素尺寸和绿色三角形中心读回 |
| 反向检查 | macOS 请求 GLES 时无法创建上下文，15 秒超时后报告失败、退出码 1；输出目录不存在时退出码 1 |

分数 DPR 下实际窗口为 800×501、framebuffer 为 1000×626，这是原生窗口几何
舍入的实测值。探针对目标逻辑尺寸允许 ±1 像素，但仍按实际窗口与 DPR 严格检查
framebuffer 尺寸。这个测试没有覆盖模型拾取。

`supportsThreadedOpenGL=false` 仅是本机报告；没有据此判断鸿蒙目标能力，
也没有测试跨线程 GL 资源上传。

### 重现主程序构建

以下在 macOS ARM64 的 UTF-8 环境执行；`qmake` 来自上述 Qt Kit。
源树用固定 commit 导出到 ASCII 路径，避免本机默认 `C.UTF-8` locale 对中文路径的处理问题。

```sh
mkdir -p /tmp/leocad-d1-src /tmp/leocad-d1-build
git archive 3e2e15b5bcd6ba87720bd2dfebffd49fd48ee3c0 | tar -x -C /tmp/leocad-d1-src
cd /tmp/leocad-d1-build
LC_ALL=en_US.UTF-8 /path/to/Qt/6.12.0/macos/bin/qmake \
  /tmp/leocad-d1-src/leocad.pro CONFIG+=release QMAKE_APPLE_DEVICE_ARCHS=arm64 \
  -after 'QMAKE_BUNDLE_DATA-=library' 'QMAKE_BUNDLE_DATA-=povray'
make -j6
```

仅取消未随仓库提供的外部 `library.bin` 和 `povray` 的 macOS bundle 复制项，
没有改变源码或关闭核心渲染。实际模型测试显式使用已随仓库内置的
`resources/library.zip`；这不是 C 的精简 1160 件库验收，也不是可分发安装包。
Qt 的 `lrelease` 在该版本依赖 QtQml，因此仅安装 qtbase/qttools 不足，
还需 qtdeclarative。

回到工作仓库执行（先创建输出目录）：

```sh
/tmp/leocad-d1-build/build/release/LeoCAD.app/Contents/MacOS/LeoCAD \
  tests/fixtures/render-baseline.ldr -l resources/library.zip \
  --image /tmp/leocad-model.png --width 800 --height 600
```

### 重现探针

```sh
python3 scripts/Build-MemberA.py --qt-prefix /path/to/Qt/6.12.0/macos --run
```

分数缩放复测：

```sh
QT_SCALE_FACTOR=0.625 build/render-probe/leocad-render-probe \
  --output build/render-probe/dpr125.json
```

本机屏幕原生 DPR=2，因此 0.625 的倍率得到 1.25。其他屏幕以 JSON 实测值为准。
完整参数和诊断范围见 [探针说明](../tests/render/README.md)。

## 证据与产物

- [构建和反向测试记录](evidence/member-a/2026-10-27/build-record.json)
- [桌面编译日志](evidence/member-a/2026-10-27/desktop-build.log)
- [探针构建及运行日志](evidence/member-a/2026-10-27/probe-build-and-run.log)
- [模型导出日志](evidence/member-a/2026-10-27/desktop-smoke.log)
- [DPR=2 JSON](evidence/member-a/2026-10-27/macos-probe.json)
- [DPR=1.25 JSON](evidence/member-a/2026-10-27/macos-probe-dpr125.json)
- [GLES 不可用的失败 JSON](evidence/member-a/2026-10-27/macos-gles-unavailable.json)
- [GL 截图](evidence/member-a/2026-10-27/macos-probe.png)
- [固定模型截图](evidence/member-a/2026-10-27/macos-model.png)

可执行文件保留在本地构建目录，未提交机器相关二进制。
`build-record.json` 保存了二进制 SHA-256 和交付源码文件哈希。
上述日志中的工作区和 Qt 安装路径已用占位符替换；命令、诊断和结果未改变。

## 剩余项与交接

1. [风险清单](MEMBER_A_RENDER_RISKS.md) 中 GL-01 至 GL-04 优先进入 D2/D3。
2. 组长将探针接入实际 Qt/HarmonyOS HAP 宿主；本地没有已验证的 Qt for HarmonyOS Kit，
   因而未交付或声称完成 ARM64 HAP。
3. 按 [真机请求](../tests/device/MEMBER_A_GL_PROBE.md) 采集目标结果。
4. 10/29 交上下文兼容补丁，10/31 真机显示模型，11/3 完成交互。
5. 依协作约定由成员 B 审核 A 的 PR，组长合入；本次没有自动批准或合并。

回退只需撤回独立诊断目标及相关记录，主程序无行为变化。
