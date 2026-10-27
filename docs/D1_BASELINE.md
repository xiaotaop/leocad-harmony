# D1 技术基线与启动记录

日期：2026-10-27

负责人：组长

上游：LeoCAD `v26.09` / `5196355d4a5adb35f14c97e7672d7a9bfd3af391`

## 当日结论

- 主界面继续使用 LeoCAD 原有 Qt Widgets，不以 ArkUI 重写。
- 构建入口迁移到根目录 `CMakeLists.txt`；桌面 CI 与鸿蒙目标共用源文件清单。
- 鸿蒙包采用 Qt 6.12 官方 CMake 部署链：`qt_add_executable(leocad)` 生成应用目标，安装 Qt for HarmonyOS 后使用 `leocad_make_hap` 生成 HAP。
- 首发只构建 `arm64-v8a`，包名暂定 `io.github.xiaotaop.leocad`，版本 `0.1.0 (1)`。
- 禁用在线更新、外部渲染和 Blender 集成，不申请首发版不需要的网络能力。

## 已检测并冻结的环境

| 组件 | D1 版本 | 状态 |
|---|---:|---|
| DevEco command-line tools | 26.0.0.851 | 已安装 |
| HarmonyOS SDK | 26.0.0.105 / API 26 | 已安装 |
| Hvigor | 6.26.8 | 已安装 |
| CMake | 3.28.2 | SDK 内已安装 |
| Ninja | 1.13.2 | SDK 内已安装 |
| Clang | 15.0.4 | SDK 内已安装 |
| Java | 23.0.2 | 已安装 |
| Qt for HarmonyOS | 6.12.0 / `ef25002ed292b132d1ed3246bfcee1d24a9cd966` | 已安装，HAP 已构建 |

完整、可提交的版本记录见 `toolchain.lock`；本机绝对路径和签名信息不进入仓库。

## 构建入口

桌面回归（已安装桌面 Qt 6.12 时）：

```powershell
cmake -S . -B build/desktop -G Ninja -DCMAKE_PREFIX_PATH=<Qt桌面版目录>
cmake --build build/desktop
```

鸿蒙构建：

```powershell
$env:OHOS_SDK_ROOT = Join-Path $env:DEVECO_SDK_HOME 'default\openharmony'
$env:QT_HARMONYOS_HVIGOR = Join-Path (Split-Path $env:DEVECO_SDK_HOME -Parent) 'bin\hvigorw.bat'
<Qt-for-HarmonyOS>/bin/qt-cmake -S . -B build/harmony -G Ninja
cmake --build build/harmony --target leocad_make_hap
```

实际 Qt 包构建时使用的 SDK 版本必须与本项目 SDK 相同，否则可能出现系统符号缺失。本机同时存在多套 command-line tools 时，不要依赖 PATH 中的 `hvigorw`；必须按上例把 `QT_HARMONYOS_HVIGOR` 指向与 `DEVECO_SDK_HOME` 同一套工具。本次已验证 6.26.8 可构建，PATH 中旧版 6.22.9 会错误报告 SDK 组件缺失。

## D1 验收与阻塞

- [x] GitHub 仓库、`origin`/只读 `upstream` 和四条开发分支已建立。
- [x] 根 CMake 包装、Qt 模块、zlib、资源、翻译与 HAP 元数据骨架已建立。
- [x] CI 已覆盖 PR 结构检查和桌面 Qt 6.12 编译回归。
- [x] SDK、编译器和构建工具版本已检测并固定。
- [x] 上架事项表已建立。
- [x] 安装 Qt 6.12 for HarmonyOS，并记录其精确版本和构建提交。
- [x] 首次执行 `leocad_make_hap`，生成 `entry-default-unsigned.hap`。

首次完成应用元数据配置后的未签名 HAP 大小为 76,961,582 字节，SHA-256 为 `EE9E0E6712FD2AA6F6148176E0377DD8C4A83FF9AE6B60275388C54E5E44FDDC`。该文件用于证明构建链闭环，不作为真机或上架包；后续须在 DevEco Studio 中配置项目签名后重新构建。

## 次日接口约定

- 成员 A 的图形兼容改动必须保持 `leocad` CMake 目标可编译。
- 成员 B 不改变公共模型接口时可直接修改 `common/`；新增源文件同步登记到 `cmake/LeoCADSources.cmake`。
- 成员 C 的 UI 和资源新增文件同样登记到源清单或 qrc；应用图标最终文件进入 `app/`，不要覆盖上游品牌资源。
- 真机安装包、日志、截图和问题编号统一进入 `tests/device/` 约定的证据索引，不提交签名密钥。
