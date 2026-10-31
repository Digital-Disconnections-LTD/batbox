"""Include the local installer matching a release package's platform."""
from pathlib import Path
import shutil


def stage_installers(source: Path, stage: Path, platform: str) -> None:
    names = (["install-windows.ps1"] if platform == "windows" else
             ["install-posix.sh", "install-linux.sh", "install-macos.sh"])
    destination = stage / "gadgets"
    destination.mkdir(parents=True, exist_ok=True)
    for name in names:
        shutil.copy2(source / "gadgets" / name, destination / name)
