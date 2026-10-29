# 成员 A D3：GLSL 与基础渲染设备交接

2026-10-29 组长补验：正常/基础双包的 shader 初始化、固定两砖显示、相机/选择、窗口变化、正常退出重启及恢复正常包已通过。固定源码、两包哈希、原始日志和未覆盖项见 [真机验收记录](MEMBER_A_D3_DEVICE_ACCEPTANCE.md)。真实透明材质、专门贴图及步骤效果样例仍未验；Issue #12 不关闭。下文保留原交接步骤。

日期：2026-10-29。提交号以本文件所在 PR 的当前代码提交为准，构建前记录完整 SHA。
本次依赖最新 main 的已合并 #11/#17；不会关闭 Issue #12。

## 包与开关

生成两个独立构建目录，先保持默认 OFF，第二包启用 `LEOCAD_SAFE_RENDER=ON`。
该 CMake 选项同时作用于主程序和可选模型测试 HAP，不改 Qt Kit、签名或平台入口。

```powershell
# 按组长已验证的工具链环境执行，路径替换为本机锁定的 Kit。
& '<QtRoot>/bin/qt-cmake.bat' -S . -B build/harmony-a-d3-basic -G Ninja `
  -DCMAKE_BUILD_TYPE=Release -DLEOCAD_SAFE_RENDER=ON
# 再调用现有脚本；脚本未覆盖该缓存选项，部署后仍由官方 CLI 生成签名。
./scripts/build-harmony-app.ps1 -SdkRoot '<CLT/sdk>' -QtRoot '<QtRoot>' `
  -BuildDirectory build/harmony-a-d3-basic -GenerateSignature -Run -Device '<目标>'
```

正常包使用另一全新目录（如 `build/harmony-a-d3-normal`）和默认 OFF，或显式
配置 `-DLEOCAD_SAFE_RENDER=OFF`。不要在同一缓存目录构建两个验收包。
运行时环境 `LEOCAD_SAFE_RENDER=1` 也可用于桌面对照；设备优先用构建选项，
不假设 `hdc shell` 的环境变量会继承到 Ability。编译强制 ON 时环境 0 不会解除。

## 真机步骤（8 步）

1. 记录代码 SHA、环境、两包 SHA-256；使用相同 Qt/SDK 与固定模型。
2. 安装正常包，冷启动并加载 `tests/fixtures/render-baseline.ldr`；记录 PID。
3. 保存原始 `LEOCAD_GL_CONTEXT` 和 `LEOCAD_GL_RENDERER` 日志。期望后者
   `passed=true,safe_render=false,programs=9,skipped_programs=0`。
4. 对默认及放大/恢复尺寸截图，操作相机和选择；正常关闭，再次启动。
5. 安装基础包，用相同模型/视点重复；期望
   `passed=true,safe_render=true,programs=6,skipped_programs=3`。
6. 确认无网格、额外光照、条件线和步骤淡化/高亮；实体、真实透明度、字体、
   基础贴图、选择/焦点提示仍可用。若无贴图/透明样例，只标记未验证。
7. 记录每轮 native main 返回值、截图和异常。基础模式隐藏网格属于显式降级，
   不是 Issue #12 的深度/遮挡根因修复，不据此关闭 #12。
8. 回传原始日志、包哈希、设备/Qt/SDK、截图和退出记录；关掉开关并重建后恢复正常路径。

不要以历史探针或旧 #11 日志替代本次代码验证。基础着色器失败会返回初始化失败；
GLES 下不会退到无 shader 的固定功能管线。失败日志包含材质、顶点/片元资源、阶段、
错误码和驱动日志。不要在验收 HAP 中故意破坏资源；负例已在宿主副本验证。

## 回退

撤回本次 A D3 提交可恢复原有初始化行为；仅恢复效果可用新构建目录或
`LEOCAD_SAFE_RENDER=OFF` 并清除运行时环境 1。默认 OFF，不保存或修改用户图形偏好。
