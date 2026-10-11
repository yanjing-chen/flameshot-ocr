#!/usr/bin/env python3

from pathlib import Path
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
CONTROL = (ROOT / "packaging/debian/control").read_text(encoding="utf-8")
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
WORKFLOW = (ROOT / ".github/workflows/build.yml").read_text(encoding="utf-8")
METAINFO_PATH = ROOT / "data/appdata/org.flameshot.Flameshot.metainfo.xml"

MAINTAINER = "Yanjing Chen <yanjing_chen@qq.com>"
REPOSITORY = "https://github.com/yanjing-chen/flameshot-ocr"

assert f"Maintainer: {MAINTAINER}" in CONTROL
assert "Maintainer: Boyuan Yang" not in CONTROL
assert f"Homepage: {REPOSITORY}" in CONTROL
assert f"Vcs-Browser: {REPOSITORY}" in CONTROL
assert f"Vcs-Git: {REPOSITORY}.git" in CONTROL

assert 'set(CPACK_PACKAGE_VENDOR "Yanjing Chen")' in CMAKE
assert f'set(CPACK_PACKAGE_HOMEPAGE_URL "{REPOSITORY}")' in CMAKE
assert f'set(CPACK_PACKAGE_CONTACT "{MAINTAINER}")' in CMAKE
assert (
    'set(GIT_API_URL "https://api.github.com/repos/'
    'yanjing-chen/flameshot-ocr/releases/latest")'
) in CMAKE

root = ET.parse(METAINFO_PATH).getroot()
assert root.findtext("name") == "Flameshot OCR"
developer = root.find("developer")
assert developer is not None
assert developer.attrib["id"] == "io.github.yanjing-chen"
assert developer.findtext("name") == "Yanjing Chen"
urls = {
    item.attrib["type"]: (item.text or "").strip()
    for item in root.findall("url")
}
assert urls["homepage"] == REPOSITORY
assert urls["bugtracker"] == f"{REPOSITORY}/issues"
assert urls["vcs-browser"] == REPOSITORY
assert root.findtext("update_contact") == "yanjing_chen_at_qq.com"

assert "package_identity_contract.py" in WORKFLOW
assert f')" = "{MAINTAINER}"' in WORKFLOW
assert f')" = "{REPOSITORY}"' in WORKFLOW

COPYRIGHT = (ROOT / "packaging/debian/copyright").read_text(encoding="utf-8")
MANPAGE = (ROOT / "data/man/man1/flameshot.1").read_text(encoding="utf-8")
assert "Boyuan Yang" in COPYRIGHT
assert "Boyuan Yang" in MANPAGE

print("DEB MAINTAINER IDENTITY       PASS")
print("CPACK CONTACT IDENTITY        PASS")
print("APPSTREAM DEVELOPER IDENTITY  PASS")
print("FORK RELEASE URLS             PASS")
print("UPSTREAM ATTRIBUTION KEPT     PASS")
