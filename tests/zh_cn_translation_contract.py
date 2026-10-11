#!/usr/bin/env python3

from collections import defaultdict
from pathlib import Path
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
TRANSLATIONS = ROOT / "data/translations/Internationalization_zh_CN.ts"

document = ET.parse(TRANSLATIONS)
duplicates = []

for context in document.getroot().findall("context"):
    context_name = context.findtext("name", default="")
    messages = defaultdict(list)

    for message in context.findall("message"):
        key = (
            message.findtext("source", default=""),
            message.findtext("comment", default=""),
            message.get("numerus", ""),
        )
        messages[key].append(message)

    for key, matches in messages.items():
        if len(matches) > 1:
            duplicates.append((context_name, key[0], len(matches)))

assert not duplicates, duplicates

ocr_context = next(
    context
    for context in document.getroot().findall("context")
    if context.findtext("name") == "OcrConf"
)
cpu_message = next(
    message
    for message in ocr_context.findall("message")
    if message.findtext("source") == "CPU"
)
cpu_translation = cpu_message.find("translation")
assert cpu_translation is not None
assert cpu_translation.text == "CPU"
assert cpu_translation.get("type") != "unfinished"

print("ZH_CN DUPLICATE MESSAGES       NONE")
print("ZH_CN CPU TRANSLATION          COMPLETE")
print("ZH_CN TRANSLATION CONTRACT: PASS")
