#!/usr/bin/env python3
import re
from pathlib import Path
import sys
from datetime import datetime

if len(sys.argv) != 2:
    print("Usage: python sync_version.py <path to LibVersion.h>")
    sys.exit(1)

header_file = Path(sys.argv[1])
if not header_file.exists():
    print(f"ERROR: Header file not found: {header_file}")
    sys.exit(1)

current_year = str(datetime.now().year)

# Files to update
PYTHON_FILES = [
    "conanfile.py"
]

# Read header and extract version
text = header_file.read_text()
major = re.search(r"#define\s+IMS_API_MAJOR\s+(\d+)", text)
minor = re.search(r"#define\s+IMS_API_MINOR\s+(\d+)", text)
patch = re.search(r"#define\s+IMS_API_PATCH\s+(\d+)", text)

if not (major and minor and patch):
    print("ERROR: Could not find IMS_API_MAJOR/MINOR/PATCH in header")
    sys.exit(1)

major_v = major.group(1)
minor_v = minor.group(1)
patch_v = patch.group(1)

version_str = f"{major_v}.{minor_v}.{patch_v}"
cs_version_str = f"{version_str}.*"

print(f"Syncing versions to: {version_str}")

# -------------------------------------------------------------------------
# Update Python files
# -------------------------------------------------------------------------
for pyfile in PYTHON_FILES:
    path = Path(pyfile)
    if not path.exists():
        print(f"WARNING: {path} not found, skipping")
        continue

    content = path.read_text()
    content_new = re.sub(
        r'(version\s*=\s*")[0-9]+\.[0-9]+\.[0-9]+(")',
        lambda m: f'{m.group(1)}{version_str}{m.group(2)}',
        content
    )
    content_new = re.sub(
        r'(version\s*:\s*")[0-9]+\.[0-9]+\.[0-9]+(")',
        lambda m: f'{m.group(1)}{version_str}{m.group(2)}',
        content_new
    )
    path.write_text(content_new)
    print(f"Updated {path}")
