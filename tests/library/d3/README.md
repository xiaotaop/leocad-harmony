# D3 接入测试

验证 `lcPiecesLibrary::ValidateBundledLibraryArchive()`：主程序在把精简库交给
`LoadPartsLibrary()` 之前用它判断归档是否真的可用。

## 为什么需要这个测试

`lcPiecesLibrary::Load()` 对不可用的归档**返回 true**：`OpenArchive()` 跳过所有
不以 `ldraw/` 开头的条目后仍无条件 `return true`。因此「启动成功」本身不能证明
校验函数有效——必须用已知的坏归档去撞它。

## 用例

| 用例 | 变体 | 期望 |
|---|---|---|
| 完整归档 | 原始 `library-lite.zip` | 通过，件数 == manifest 的 `counts.parts` |
| 根目录错误 | `wrongroot.zip` | 拒绝，提示 `outside ldraw/` |
| 缺色表 | `nocfg.zip` | 拒绝，提示 `ldconfig` |
| 引用断裂 | `dangling.zip` | 拒绝，提示 `references a file` |
| 截断 | `truncated.zip` | 拒绝，提示 `expected` |
| 文件不存在 | — | 拒绝，提示 `not found` |

变体由 `prepare_variants.py` 从当前发布归档生成，不入库：归档变了，变体跟着变，
不会过期。

## 运行

```bash
tests/library/d3/prepare_variants.py
mkdir -p build/d3 && cd build/d3
qmake ../../d3-tests.pro CONFIG+=debug
make -j4 && ./build/debug/leocad-d3-tests
```
