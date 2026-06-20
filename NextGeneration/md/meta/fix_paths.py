#!/usr/bin/env python3
"""One-shot path repair after folder reorganize. Safe to re-run."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

REPLACEMENTS = [
    ("assets/main/", "assets/heroes/"),
    ("../../assets/main/", "../../assets/heroes/"),
    ("assets/wiki/architecture/", "assets/wiki/stack/architecture/"),
    ("assets/wiki/field_die/", "assets/wiki/stack/field_die/"),
    ("assets/wiki/data_bus/", "assets/wiki/stack/data_bus/"),
    ("assets/wiki/ammoos/", "assets/wiki/stack/ammoos/"),
    ("assets/wiki/chips/", "assets/wiki/stack/chips/"),
    ("assets/wiki/canvas_shaders/", "assets/wiki/stack/canvas_shaders/"),
    ("assets/wiki/field_fabric/", "assets/wiki/physics/field_fabric/"),
    ("assets/wiki/thermo_accountant/", "assets/wiki/physics/thermo_accountant/"),
    ("assets/wiki/observability/", "assets/wiki/physics/observability/"),
    ("assets/wiki/getting_started/", "assets/wiki/start/getting_started/"),
    ("assets/wiki/glossary/", "assets/wiki/start/glossary/"),
    ("assets/wiki/build_and_run/", "assets/wiki/start/build_and_run/"),
    ("../../assets/wiki/architecture/", "../../assets/wiki/stack/architecture/"),
    ("../../assets/wiki/field_die/", "../../assets/wiki/stack/field_die/"),
    ("../../assets/wiki/data_bus/", "../../assets/wiki/stack/data_bus/"),
    ("../../assets/wiki/ammoos/", "../../assets/wiki/stack/ammoos/"),
    ("../../assets/wiki/chips/", "../../assets/wiki/stack/chips/"),
    ("../../assets/wiki/canvas_shaders/", "../../assets/wiki/stack/canvas_shaders/"),
    ("../../assets/wiki/field_fabric/", "../../assets/wiki/physics/field_fabric/"),
    ("../../assets/wiki/thermo_accountant/", "../../assets/wiki/physics/thermo_accountant/"),
    ("../../assets/wiki/observability/", "../../assets/wiki/physics/observability/"),
    ("../../assets/wiki/getting_started/", "../../assets/wiki/start/getting_started/"),
    ("../../assets/wiki/glossary/", "../../assets/wiki/start/glossary/"),
    ("../../assets/wiki/build_and_run/", "../../assets/wiki/start/build_and_run/"),
    ("md/articles/main_hero_", "md/articles/heroes/main_hero_"),
    ("md/articles/wiki_", "md/articles/companions/wiki_"),
    ("articles/main_hero_", "articles/heroes/main_hero_"),
    ("articles/wiki_", "articles/companions/wiki_"),
    ("articles/MEMORIUMS.md", "meta/MEMORIUMS.md"),
    ("articles/ON_IMAGE_TEXT.md", "meta/ON_IMAGE_TEXT.md"),
    ("articles/PAGE_CASTS.md", "meta/PAGE_CASTS.md"),
    ("articles/WIKI_EMBED_SNIPPETS.md", "meta/WIKI_EMBED_SNIPPETS.md"),
    # memes: from md/* subdirs need three hops to SG/
    ("../../memes/", "../../../memes/"),
    ("`../../memes/", "`../../../memes/"),
    ("(../../memes/", "(../../../memes/"),
]

# FOLDER path in PAGE_CASTS
FOLDER_MAP = {
    "wiki/<page>/": "wiki/stack|physics|start/<page>/",
}


def patch_file(path: Path) -> bool:
    text = path.read_text()
    orig = text
    for old, new in REPLACEMENTS:
        text = text.replace(old, new)
    if text != orig:
        path.write_text(text)
        return True
    return False


def main():
    changed = []
    for path in ROOT.rglob("*"):
        if path.suffix in {".md", ".py", ".gitignore"} and path.name != "fix_paths.py":
            if patch_file(path):
                changed.append(path.relative_to(ROOT))
    print(f"Patched {len(changed)} files")
    for p in sorted(changed):
        print(" ", p)


if __name__ == "__main__":
    main()