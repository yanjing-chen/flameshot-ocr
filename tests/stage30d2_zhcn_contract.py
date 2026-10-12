#!/usr/bin/env python3

from pathlib import Path
import xml.etree.ElementTree as ET

TS = Path("data/translations/Internationalization_zh_CN.ts")

tree = ET.parse(TS)
root = tree.getroot()

catalog = {}
for ctx in root.findall("context"):
    name = ctx.findtext("name") or ""
    for msg in ctx.findall("message"):
        source = msg.findtext("source")
        tr = msg.find("translation")
        if source is None or tr is None:
            continue
        catalog[(name, source)] = (tr.text or "", tr.get("type"))

required = {
    ("CaptureWidget", "Flameshot OCR - Window Editor"):
        "Flameshot OCR - 截图编辑器",
    ("CaptureWidget", "OCR Result"):
        "OCR 识别结果",
    ("CaptureWidget", "Save Text..."):
        "保存文本…",
    ("CaptureWidget", "Elapsed: %1 s | %2 characters | %3 characters/s"):
        "用时：%1 秒 | %2 个字符 | %3 字符/秒",
    ("CaptureWidget", "Model speed: %1 token/s"):
        "模型速度：%1 token/秒",
    ("CaptureWidget", "Elapsed time includes Runtime readiness and model loading."):
        "用时包含 Runtime 就绪检查和模型加载时间。",
    ("CaptureWidget", "Save OCR result"):
        "保存 OCR 识别结果",
    ("CaptureWidget", "Text files (*.txt);;All files (*)"):
        "文本文件 (*.txt);;所有文件 (*)",
    ("VisualsEditor", "Reset Order"):
        "恢复默认顺序",
    ("VisualsEditor", "Toolbar alignment"):
        "工具栏对齐",
    ("VisualsEditor", "Left"):
        "左对齐",
    ("VisualsEditor", "Center"):
        "居中",
    ("VisualsEditor", "Right"):
        "右对齐",
}

for key, expected in required.items():
    assert key in catalog, f"missing translation entry: {key}"
    actual, tr_type = catalog[key]
    assert tr_type != "unfinished", f"unfinished: {key}"
    assert actual == expected, f"{key}: {actual!r} != {expected!r}"
    print(f"{key[1]:<58} PASS")

print("\nSTAGE 3.0-D.2 ZH_CN CONTRACT: PASS")
