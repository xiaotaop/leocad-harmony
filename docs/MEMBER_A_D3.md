# 成员 A：2026-10-29（D3）交付

已完成上下文/Surface 证据核对、首轮 GLSL 失败处理和基础渲染降级开关。
本次基线为 `fbcdc5e11be8a9dc46471b8ac0990cb603982740`，504 个远端 blob、
完整 tree 与签名 commit 均校验一致。代码与宿主验证已完成；本次补丁尚待组长真机复验。

## 远程与审核

- A 的 D2 #9 已由 B 批准并合入；生产上下文和 Surface 的设备验证已随组长 #11 归档。
- 已审核组长 #11 的 `90f790ca43335a13672697f23b88bc29f9bacde1` 并提交正式批准，
  [审核记录](https://github.com/xiaotaop/leocad-harmony/pull/11#pullrequestreview-5686453154)。
  #11 于 10/29 09:05（北京时间）合并。原始两轮 PID44809/45162 均正常退出且共享成功；
  12 个证据文件哈希一致，模型测试 PID43199 为50/0/0，日志和 native main 均为0。
- #17/#18/#19 已进入 main，C 的精简库/UI 和 B 的 D3 集成记录包含在本次基线。
  这些功能不重算为 A 的交付；旧 #1 仍未合入。检查时没有组长新的开放 PR。

## 实现

- `lc_shaderprogram.h` 集中原有 GLSL 前缀和九组材质清单。默认源码及桌面/GLES 路线不变。
  生产和测试使用同一资源、编译及链接实现；Release 也检查每个阶段，失败时删除
  shader/program，不对 program0 查询 uniform。
- `lcContext::CreateResources/InitializeRenderer` 传播失败，清理已创建程序和离屏上下文，
  在字体/视图资源分配前退出。日志 `LEOCAD_GL_RENDERER` 包含材质、资源、阶段与驱动消息。
- 默认 OFF 的 `LEOCAD_SAFE_RENDER` CMake 选项及运行时 `LEOCAD_SAFE_RENDER=1`
  提供基础模式。它强制 Flat，跳过额外光照与条件线 shader，关闭步骤淡化/高亮及网格。
  主程序与模型测试 HAP 的编译选项一致；用户偏好不写回，不支持进程内热切换。
- 基础模式保留实体、普通边线、真实透明度、基础贴图/字体、相机及选择/焦点提示。
  它不是关闭 GLES 着色器，也不处理异步上传、资源重建、输入和网格根因。

## 本机验证

macOS27.0.1 / ARM64 / Apple M4 / Qt6.12 / AppleClang21 / Release。

- 完整主程序构建通过；CMake基础模式配置的82条编译命令均包含 `LC_SAFE_RENDER=1`。
- 共享上下文回归：13 passed / 0 failed / 0 skipped。
- 真实 GL 着色器测试：17 passed / 0 failed / 0 skipped。包含九组材质、资源缺失、
  顶点/片元编译错误、接口链接错误、失败恢复及基础 shader FBO 绿色像素读回。
- 完整 Release 主程序7个场景通过：正常和基础模式均生成800×600两砖图；额外光照
  shader损坏时正常模式退出1、基础模式退出0；基础shader损坏时两种模式均退出1；
  恢复资源后正常模式再次退出0。破坏操作仅在 `/tmp` 构建副本进行，仓库资源不改。
- 正常模式日志9个程序/跳过0，基础模式6/3；两张输出图已人工查看，红蓝实体均可见。
- 沙箱内首次运行测试无法连接macOS窗口服务而崩溃；在真实图形会话重跑后17/17通过。
  该环境失败不算通过记录。完整应用测试同样在真实图形会话运行。
- CI 增加 Mesa 桌面GL及GLES3两种前缀；当前 PR 最新 CI 状态在远端查看。
  本机macOS结果不替代Mesa/GLES或鸿蒙 GPU 验证。

证据位于 [2026-10-29](evidence/member-a/2026-10-29/build-record.json)，包含测试报告、
完整程序场景结果、必要图形日志和两张模型图。context回归结果随同目录归档。

## 真机与后续

[设备交接](MEMBER_A_D3_DEVICE_HANDOFF.md) 提供双包构建和8步验证。旧 #11 的代码
`f0dfce86` 只验证D2生产context/Surface，不能证明本次shader失败保护和基础模式已上机。
#12 网格覆盖仍按10/30–10/31修根因；隐藏网格仅是本次明确记录的临时降级。
本 PR 由成员 B 审核，组长合入；不自行批准或合并本人的工作。
