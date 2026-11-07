# D12 一致性复核证据

> 成员 C，2026-11-07。基线 `96d14f76`。只记录实际跑过的命令与输出。

## 1. 权限 `reason` 实测

```bash
python3 -c "import json; d=json.load(open('docs/evidence/leader/2026-11-05/d10-device/packaged-module.json'));
[print(p['name'], '->', {k:v for k,v in p.items() if k!='name'}) for p in d['module']['requestPermissions']]"
```

输出（原文）：

```
ohos.permission.STORE_PERSISTENT_DATA -> {}
ohos.permission.FILE_ACCESS_PERSIST -> {}
ohos.permission.PREPARE_APP_TERMINATE -> {}
ohos.permission.READ_PASTEBOARD -> {'reason': '$string:qt_permission_reason_read_pasteboard',
                                     'reasonId': 16777223,
                                     'usedScene': {'abilities': ['QAbility'], 'when': 'always'}}
ohos.permission.GET_FILE_ICON -> {}
ohos.permission.PRINT -> {}
```

结论：6 项中 5 项无 `reason`；唯一有 `reason` 的是 Qt 模板字符串键。

## 2. 隐私政策权限节缺失

```bash
grep -c '权限' docs/release/隐私政策.md   # 修改前
```

输出：`0`（零命中）。修改后新增 §2.1。

## 3. 零件数实测

```bash
python3 -c "import json; print(json.load(open('assets/ldraw-lite/manifest.json'))['counts'])"
```

输出：`{'parts': 1160, 'subparts': 669, 'primitives': 911, 'config': 1, 'total_files': 2741}`

## 4. 三份材料权限集合

```bash
grep -o 'ohos\.permission\.[A-Z_]*' docs/release/权限说明.md | sort -u
grep -o 'ohos\.permission\.[A-Z_]*' "docs/release/商店文案.md" | sort -u
grep -o 'ohos\.permission\.[A-Z_]*' docs/release/隐私政策.md | sort -u
```

修改后三者集合一致（各 6 项声明 + 2 项明确不申请）。

## 5. 全套 Python 检查

```bash
for f in scripts/library/test_*.py scripts/library/verify_*.py; do python3 "$f"; done
```

| 脚本 | 项数 | exit |
|---|---:|---:|
| test_export_part_list.py | 8 | 0 |
| test_failure_log_retention.py | 24 | 0 |
| test_network_scope.py | 13 | 0 |
| test_packaged_resources.py | 5 | 0 |
| test_permission_scope.py | 5 | 0 |
| test_release_consistency.py | 5 | 0 |
| test_release_materials.py | 4 | 0 |
| verify_sample_model.py | 6 | 0 |
| **合计** | **70** | **0** |

## 6. 变异验证

| 变异 | 结果 |
|---|---|
| 删掉隐私政策 §2.1 权限节 | exit=1，捕获 |
| 隐私政策中把 `ohos.permission.PRINT` 改名 | exit=1，捕获 |
| 商店文案零件数 1160 → 1500 | exit=1，捕获 |
| "不是完整库" → "是完整库" | exit=1，捕获 |
| "本应用非官方发布版本" → "官方发布版本" | exit=1，捕获 |
| 还原 | exit=0 |

## 7. RC1 包与截图：不存在

```bash
find . -name "*.hap" -not -path "./.devtools/*"    # 无输出
python3 -c "import json; print(json.load(open('docs/evidence/leader/2026-11-05/d10-device/packaged-module.json'))['app']['debug'])"
```

输出：`True`。

说明：最新设备批次是 2026-11-05 的 D10，仍是 debug 包；没有 RC1 release 签名包，
因此 D12 的截图无法进行。这不是省略，是前提不成立（D11 拍摄前提第 1 条要求 RC1 release 签名包）。

## 8. 未跑项

| 项 | 原因 |
|---|---|
| 真机截图 6 张 | 无 RC1 包、无真机（组长持有） |
| 权限 reason 实际展示效果 | 需 DevEco 工程改后重新打包 |
| 各权限 system_grant/user_grant 分级 | 无 HarmonyOS 分级文档，不猜 |
| 真实 HAP 内的一致性 | 需 RC0 解包（D11 记为 B-7） |
