"""Combine local build databases for VS Code C/C++ IntelliSense.

Run after configuring or rebuilding firmware, provisioning, preview, or tests:
    python3 tools/update_editor_commands.py

The output is ignored because compile commands contain local paths and the
provisioning build command contains the passkey supplied at build time.
"""

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / ".vscode" / "compile_commands.json"
DATABASES = (
    ROOT / "firmware" / "build" / "app" / "compile_commands.json",
    ROOT / "firmware" / "build-provision" / "provision" / "compile_commands.json",
    ROOT / "firmware" / "build-ui-preview" / "compile_commands.json",
    ROOT / "firmware" / "build-test" / "compile_commands.json",
)


def main() -> None:
    commands = {}
    sources = []

    for database in DATABASES:
        if not database.is_file():
            continue
        entries = json.loads(database.read_text(encoding="utf-8"))
        for entry in entries:
            # The app build wins for shared firmware sources. Provisioning,
            # preview, and tests supply commands for their own source files.
            commands.setdefault(str(Path(entry["file"]).resolve()), entry)
        sources.append(str(database.relative_to(ROOT)))

    if not sources:
        raise SystemExit("No build databases found; configure a build first.")

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(
        json.dumps(list(commands.values()), indent=2) + "\n", encoding="utf-8"
    )
    print(f"Wrote {len(commands)} commands to {OUTPUT}")
    print("From: " + ", ".join(sources))


if __name__ == "__main__":
    main()
