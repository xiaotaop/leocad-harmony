#!/usr/bin/env python3
"""Fill in the Simplified Chinese translations that the first release needs.

`lupdate` regenerated `resources/leocad_zh_CN.ts` against the current sources, so
the previously checked-in file (generated from an older revision) no longer
matched: 702 strings came out untranslated. This script carries the translations
for the strings the release actually shows, keyed by (context, source), and
leaves everything else marked unfinished instead of inventing text.

Run after `lupdate`:
    source .devtools/env.sh
    lupdate leocad.pro -no-obsolete -locations none
    python3 scripts/l10n/fill_zh_cn.py
    scripts/l10n/check_translations.py

Deliberately not translated here (recorded in docs/release/已知问题.md):
  * command-line option help in the lcApplication context — not reachable from
    the graphical application;
  * Blender/POV-Ray dialog strings — those dialogs are out of first-release scope.
"""

from __future__ import annotations

import sys
import xml.etree.ElementTree as ET
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
TS_FILE = REPO_ROOT / "resources" / "leocad_zh_CN.ts"

# context -> {english source: chinese translation}
TRANSLATIONS: dict[str, dict[str, str]] = {
    "Categories": {
        "Animal": "动物",
        "Antenna": "天线",
        "Arch": "拱形",
        "Bar": "杆件",
        "Baseplate": "底板",
        "Boat": "船",
        "Brick": "砖",
        "Container": "容器",
        "Door and Window": "门窗",
        "Electric": "电子",
        "Hinge and Bracket": "铰链与支架",
        "Hose": "软管",
        "Minifig": "人仔",
        "Miscellaneous": "其他零件",
        "Other": "其它",
        "Panel": "面板",
        "Plant": "植物",
        "Plate": "板",
        "Round": "圆形件",
        "Sign and Flag": "标志与旗帜",
        "Slope": "斜面",
        "Sticker": "贴纸",
        "Support": "支撑件",
        "Technic": "科技",
        "Tile": "光面砖",
        "Train": "火车",
        "Tyre and Wheel": "轮胎与轮子",
        "Vehicle": "车辆",
        "Windscreen": "挡风玻璃",
        "Wedge": "楔形件",
        "Wing": "机翼",
    },
    "Colors": {
        "Black": "黑色",
        "Blue": "蓝色",
        "Green": "绿色",
        "Dark Turquoise": "深青绿",
        "Red": "红色",
        "Dark Pink": "深粉红",
        "Brown": "棕色",
        "Light Grey": "浅灰",
        "Dark Grey": "深灰",
        "Light Blue": "浅蓝",
        "Bright Green": "亮绿",
        "Light Turquoise": "浅青绿",
        "Salmon": "鲑鱼红",
        "Pink": "粉红",
        "Yellow": "黄色",
        "White": "白色",
        "Light Green": "浅绿",
        "Light Yellow": "浅黄",
        "Tan": "米色",
        "Light Violet": "浅紫",
        "Purple": "紫色",
        "Light Purple": "浅紫罗兰",
        "Orange": "橙色",
        "Magenta": "洋红",
        "Lime": "青柠绿",
        "Dark Tan": "深米色",
        "Bright Pink": "亮粉红",
        "Medium Blue": "中蓝",
        "Nougat": "肉色",
        "Sand Blue": "沙蓝",
        "Sand Green": "沙绿",
        "Sand Red": "沙红",
        "Dark Red": "深红",
        "Dark Blue": "深蓝",
        "Dark Green": "深绿",
        "Trans Clear": "透明",
        "Trans Black": "透明黑",
        "Trans Red": "透明红",
        "Trans Blue": "透明蓝",
        "Trans Green": "透明绿",
        "Trans Yellow": "透明黄",
        "Trans Orange": "透明橙",
        "Trans Dark Blue": "透明深蓝",
        "Trans Light Blue": "透明浅蓝",
        "Trans Neon Green": "透明荧光绿",
        "Trans Neon Orange": "透明荧光橙",
        "Chrome Silver": "镀铬银",
        "Chrome Gold": "镀铬金",
        "Main Color": "主色",
        "Edge Color": "边缘色",
        "No Color": "无色",
        "Stud Cylinder Color": "凸点柱色",
    },
    "lcMainWindow": {
        "&Render": "渲染(&R)",
        "Change Selection Color": "修改选中零件颜色",
        "Export HTML": "导出 HTML",
        "POV-Ray Export": "POV-Ray 导出",
        "Export 3DS": "导出 3DS",
        "Export COLLADA": "导出 COLLADA",
        "Export BrickLink": "导出 BrickLink",
        "Export CSV": "导出 CSV",
        "Export Wavefront": "导出 Wavefront",
        "Nothing to select.": "没有可选择的项目。",
        "%n Object(s) selected": "已选择 %n 个对象",
        "Save Project": "保存模型",
        "Save changes to '%1'?": "要保存对 '%1' 的更改吗？",
        "The file '%1' has been modified by another application, do you want to reload it?":
            "文件 '%1' 已被其它程序修改，是否重新载入？",
        "in group '%1'": "（组：%1）",
    },
    "lcPartSelectionWidget": {
        "Filter Parts": "搜索零件",
        "Icons": "图标",
        "Lock Color": "锁定颜色",
        "Filter": "筛选",
        "Regular Expression": "正则表达式",
        "Part ID": "零件编号",
        "Part Description": "零件描述",
        "No parts in this category.": "此分类下没有可用零件。",
        "No parts library was found. Reinstall the application, or check the application data folder.":
            "未找到零件库。请重新安装应用，或检查应用数据目录。",
        "No parts available.": "没有可用的零件。",
        "No parts match this search. Try a different keyword.":
            "没有匹配的零件，请尝试其它关键词。",
        "Show Part Names": "显示零件名称",
        "Show Decorated Parts": "显示图案零件",
        "Show Part Aliases": "显示零件别名",
        "Match Case": "区分大小写",
        "Options": "选项",
        "Edit Palettes...": "编辑收藏夹...",
        "Add to Favorites": "加入收藏夹",
        "Remove from Favorites": "从收藏夹移除",
        "All Parts": "所有零件",
        "In Use": "使用中",
        "Submodels": "子模型",
        "Favorites": "收藏夹",
    },
    "lcPartSelectionListView": {
        "Open Submodel": "打开子模型",
        "Add to Favorites": "加入收藏夹",
        "Remove from Favorites": "从收藏夹移除",
        "No Icon": "无图标",
        "Small Icons": "小图标",
        "Medium Icons": "中图标",
        "Large Icons": "大图标",
        "Extra Large Icons": "特大图标",
        "List Mode": "列表模式",
        "Fixed Color": "固定颜色",
    },
    "lcModel": {
        "Cut": "剪切",
        "Paste": "粘贴",
        "Duplicate Pieces": "复制零件",
        "Duplicate": "创建副本",
        "Insert Step": "插入步骤",
        "Remove Step": "删除步骤",
        "Group Selection": "编组所选",
        "Group": "编组",
        "Ungroup Selection": "取消编组所选",
        "Ungroup": "取消编组",
        "No groups selected.": "未选择组。",
        "Edit Groups": "编辑组",
        "Add Piece": "添加零件",
        "Selection": "选择",
        "Delete": "删除",
        "Reset Pivot Point": "重置轴心点",
        "Remove Key Frames": "删除关键帧",
        "Add Control Point": "添加控制点",
        "Remove Control Point": "删除控制点",
        "Show Earlier": "显示更早步骤",
        "Show Later": "显示更晚步骤",
        "Show Pieces": "显示零件",
        "Change Step": "更改步骤",
        "Move to Model": "移动到模型",
        "Inline Model": "内联模型",
        "Add KeyFrame": "添加关键帧",
        "Remove KeyFrame": "删除关键帧",
        "Paint": "上色",
        "Set Show Step": "设为显示步骤",
        "Set Hide Step": "设为隐藏步骤",
        "Change Projection": "切换投影",
        "Hide Pieces": "隐藏零件",
        "Unhide Pieces": "取消隐藏零件",
        "Replace Piece(s)": "替换零件",
        "Add Camera": "添加相机",
        "Add Point Light": "添加点光源",
        "Add Spot Light": "添加聚光灯",
        "Add Directional Light": "添加平行光",
        "Add Area Light": "添加区域光",
        "Zoom Extents": "适应窗口",
        "Change Model Properties": "修改模型属性",
        "Piece Array": "零件阵列",
        "Add Minifig": "添加人仔",
    },
    "Menu": {
        "&POV-Ray Render...": "POV-Ray 渲染(&P)...",
        "&Blender Render...": "Blender 渲染(&B)...",
        "&Open In Blender...": "在 Blender 中打开(&O)...",
        "Point Light": "点光源",
        "Spot Light": "聚光灯",
        "Directional Light": "平行光",
        "Area Light": "区域光",
        "Focus Next Connection": "聚焦下一个连接点",
        "Focus Previous Connection": "聚焦上一个连接点",
        "Rotate Train Track": "旋转火车轨道",
    },
    "Status": {
        "Render the current model using Blender": "使用 Blender 渲染当前模型",
        "Open the current model in Blender": "在 Blender 中打开当前模型",
        "Add new point light sources to the model": "向模型中添加点光源",
        "Add new spot lights to the model": "向模型中添加聚光灯",
        "Add new omnidirectional light sources to the model": "向模型中添加全向光源",
        "Add new area light sources to the model": "向模型中添加区域光源",
        "Focus the next connection of the currently selected track": "聚焦当前选中轨道的下一个连接点",
        "Focus the previous connection of the currently selected track": "聚焦当前选中轨道的上一个连接点",
        "Rotate the focused train track arount its last connection": "围绕最后一个连接点旋转当前轨道",
    },
    "lcApplication": {
        "LeoCAD Version %1": "LeoCAD 版本 %1",
        "LeoCAD Continuous Build %1": "LeoCAD 持续构建 %1",
        "Compiled %1": "编译于 %1",
        "Error creating OpenGL context.": "创建 OpenGL 上下文失败。",
        "No file name specified.": "未指定文件名。",
        "Error: model '%1' does not exist.": "错误：模型 '%1' 不存在。",
        "Saved '%1'.": "已保存 '%1'。",
        "The file '%1' is not readable.": "文件 '%1' 无法读取。",
        "LeoCAD could not load Parts Library.\n\nPlease visit https://www.leocad.org for information on how to download and install a library.":
            "LeoCAD 无法加载零件库。\n\n有关下载和安装零件库的信息，请访问 https://www.leocad.org。",
        "LeoCAD could not find a compatible Parts Library so only a small number of parts will be available.\n\nPlease visit https://www.leocad.org for information on how to download and install a library.":
            "LeoCAD 未找到兼容的零件库，因此只能使用少量零件。\n\n有关下载和安装零件库的信息，请访问 https://www.leocad.org。",
        "Unknown option: '%1'.": "未知选项：'%1'。",
        "The built-in parts library could not be loaded, so only a limited set of parts is available. Try reinstalling the application.":
            "内置零件库加载失败，仅能使用少量零件。请尝试重新安装应用。",
        "The parts library could not be loaded. Try reinstalling the application.":
            "零件库加载失败。请尝试重新安装应用。",
        "LeoCAD Version %1\n": "LeoCAD 版本 %1\n",
        "LeoCAD Continuous Build %1\n": "LeoCAD 持续构建 %1\n",
        "Compiled %1\n": "编译于 %1\n",
        "No file name specified.\n": "未指定文件名。\n",
        "Error creating OpenGL context.\n": "创建 OpenGL 上下文失败。\n",
        "Error: model '%1' does not exist.\n": "错误：模型 '%1' 不存在。\n",
        "Saved '%1'.\n": "已保存 '%1'。\n",
        "The file '%1' is not readable.\n": "文件 '%1' 无法读取。\n",
        "Unknown option: '%1'.\n": "未知选项：'%1'。\n",
        "Command line image export is not supported on iOS.\n": "iOS 上不支持命令行图片导出。\n",
        "Not enough parameters for the '%1' option.\n": "选项 '%1' 的参数不足。\n",
        "Invalid parameter value specified for the '%1' option: '%2'.\n": "选项 '%1' 的参数值无效：'%2'。\n",
        "Invalid parameter value(s) specified for the '%1' option: limits are +/- 360.\n": "选项 '%1' 的参数值无效：范围为 ±360。\n",
        "The camera position is too close to the camera target of the '%1' option.\n": "选项 '%1' 的相机位置与目标点过近。\n",
        "Invalid up vector length specified for the '%1' option.\n": "选项 '%1' 的上方向向量长度无效。\n",
        "The up vector is parallel to the front vector specified for the '%1' option.\n": "选项 '%1' 的上方向向量与前方向向量平行。\n",
        "Invalid parameter value(s) specified for the '%1' option: requirements are: 1 <= <near> < <far>.\n": "选项 '%1' 的参数值无效：要求 1 <= <near> < <far>。\n",
        "High contrast stud and edge color settings are ignored when -aec or --automate-edge-color is set.\n": "设置了 -aec 或 --automate-edge-color 时，高对比度凸点色与边缘色设置将被忽略。\n",
        "Automate edge color is required for the '%1' option but is not enabled.\n": "选项 '%1' 需要启用自动边缘色，但当前未启用。\n",
        "High contrast stud style is required for the '%1' option but is not enabled.\n": "选项 '%1' 需要启用高对比度凸点样式，但当前未启用。\n",
    },
    "Project": {
        "Error reading file '%1':\n%2": "读取文件 '%1' 失败：\n%2",
        "Error loading file '%1':\nFile format is not recognized.": "加载文件 '%1' 失败：\n无法识别文件格式。",
    },
    "QMessageBox": {
        "Existing file %1 detected.": "检测到已存在的文件 %1。",
        '"%1"<br>This file already exists.<br>Replace existing file?':
            '"%1"<br>此文件已存在。<br>是否替换现有文件？',
    },
    "lcGroupDialog": {
        "A group with this name already exists.": "已存在同名组。",
    },
    "lcFindReplaceWidget": {
        "Search Color": "搜索颜色",
        "Search Part": "搜索零件",
        "Replacement Color": "替换颜色",
        "Replacement Part": "替换零件",
        "None": "无",
        "Close": "关闭",
    },
    "lcPreferencesDialog": {
        "Blender LDraw Render:": "Blender LDraw 渲染：",
        "Addon Settings...": "插件设置...",
        "Text Halo Color:": "文字光晕颜色：",
        "Select Text Halo Color": "选择文字光晕颜色",
    },
    "lcQPropertiesDialog": {
        "Select Text Halo Color": "选择文字光晕颜色",
    },
    "Action": {
        "File.Render.POVRay": "文件.渲染.POV-Ray",
        "File.Render.Blender": "文件.渲染.Blender",
        "File.Render.OpenInBlender": "文件.渲染.在Blender中打开",
        "Edit.Tool.PointLight": "编辑.工具.点光源",
        "Edit.Tool.SpotLight": "编辑.工具.聚光灯",
        "Edit.Tool.DirectionalLight": "编辑.工具.平行光",
        "Edit.Tool.AreaLight": "编辑.工具.区域光",
        "Piece.TrainTrack.FocusNext": "零件.火车轨道.聚焦下一个连接点",
        "Piece.TrainTrack.FocusPrevious": "零件.火车轨道.聚焦上一个连接点",
        "Piece.TrainTrack.Rotate": "零件.火车轨道.旋转",
    },
}


def main() -> int:
    if not TS_FILE.exists():
        print(f"error: {TS_FILE} missing", file=sys.stderr)
        return 1

    tree = ET.parse(TS_FILE)
    root = tree.getroot()

    filled = 0
    unknown_contexts: set[str] = set()
    known_contexts: set[str] = set()

    for context in root.iter("context"):
        name = context.findtext("name", "")
        table = TRANSLATIONS.get(name)
        if table is None:
            continue
        known_contexts.add(name)

        for message in context.iter("message"):
            source = message.findtext("source", "")
            translation = message.find("translation")
            if translation is None:
                continue

            existing = (translation.text or "").strip()
            finished = translation.get("type", "finished") != "unfinished"
            if existing and finished:
                # Keep human translations that already exist; this script only
                # fills gaps.
                continue

            if source in table:
                translation.text = table[source]
                if "type" in translation.attrib:
                    del translation.attrib["type"]
                filled += 1

    for name in TRANSLATIONS:
        if name not in known_contexts:
            unknown_contexts.add(name)

    # Plural messages are stored as <numerusform> children. Writing plain text
    # into them produces a file lrelease rejects, so normalise every
    # numerus message here: keep the first existing form, otherwise fall back to
    # the plain text that was set above.
    normalised = 0
    for message in root.iter("message"):
        if message.get("numerus") != "yes":
            continue
        translation = message.find("translation")
        if translation is None:
            continue
        forms = [form.text.strip() for form in translation.findall("numerusform")
                 if form.text and form.text.strip()]
        plain = (translation.text or "").strip()
        text = forms[0] if forms else plain
        for form in translation.findall("numerusform"):
            translation.remove(form)
        translation.text = None
        if text:
            numerus_form = ET.SubElement(translation, "numerusform")
            numerus_form.text = text
            normalised += 1
        if "type" in translation.attrib:
            del translation.attrib["type"]

    # Write back with the same declaration lupdate produces, LF endings, no BOM.
    body = ET.tostring(root, encoding="unicode")
    header = '<?xml version="1.0" encoding="utf-8"?>\n<!DOCTYPE TS>\n'
    TS_FILE.write_text(header + body + "\n", encoding="utf-8",
                       newline="\n")

    print(f"filled {filled} translations, normalised {normalised} plural forms")
    if unknown_contexts:
        print(f"warning: contexts not present in the .ts: "
              f"{', '.join(sorted(unknown_contexts))}", file=sys.stderr)

    remaining = sum(1 for t in root.iter("translation")
                    if t.get("type") == "unfinished")
    print(f"still unfinished: {remaining}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
