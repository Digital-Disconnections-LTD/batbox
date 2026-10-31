"""Copy project and dependency license records into a release package."""
from pathlib import Path
import shutil


def stage_notices(source: Path, stage: Path) -> None:
    for name in ("LICENSE", "THIRD_PARTY_NOTICES.md"):
        shutil.copyfile(source / name, stage / name)
    for directory in (source / "src", source / "third_party"):
        for path in sorted(directory.rglob("*")):
            if not path.is_file():
                continue
            if (path.name.lower().startswith(("license", "notice", "copying", "unicode-license"))
                    or "licenses" in path.parts or "notices" in path.parts):
                destination = stage / path.relative_to(source)
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(path, destination)
