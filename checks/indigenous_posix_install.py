#!/usr/bin/env python3
"""Exercise POSIX installers using isolated homes and fixture executables."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ("install-macos.sh", "install-linux.sh", "install-posix.sh")


def write(path, content, executable=False):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content)
    if executable:
        path.chmod(0o755)


def installers(destination):
    (destination / "gadgets").mkdir(parents=True, exist_ok=True)
    for name in SCRIPTS:
        shutil.copy2(ROOT / "gadgets" / name, destination / "gadgets" / name)


def invoke(package, platform, environment, *arguments, success=True):
    result = subprocess.run(
        ["bash", str(package / "gadgets" / f"install-{platform}.sh"), *arguments],
        env=environment, cwd=package.parent, text=True, capture_output=True,
        timeout=30,
    )
    if (result.returncode == 0) != success:
        raise AssertionError(result.stdout + result.stderr)
    return result


def package_case(temporary, platform, shell):
    area = temporary / f"{platform} {shell} package"
    package = area / "extracted archive"
    home = area / "user's home"
    shim = area / "command shims"
    callers = area / "caller workspace"
    callers.mkdir(parents=True)
    installers(package)
    write(package / "bin/batbox", '#!/bin/sh\nprintf "%s\\n" "$PWD" "$@"\n', True)
    write(package / "LICENSE", "fixture MIT notice\n")
    write(package / "THIRD_PARTY_NOTICES.md", "fixture dependency notices\n")
    write(package / "lib/example.1.dylib", "fixture runtime\n")
    (package / "lib/example.dylib").symlink_to("example.1.dylib")
    write(package / "etc/ssl/certs/bundle.crt", "fixture certificates\n")
    write(package / "data/nested/asset.txt", "fixture data\n")
    write(shim / "uname", f"#!/bin/sh\nprintf '%s\\n' {'Darwin' if platform == 'macos' else 'Linux'}\n", True)
    write(home / ".config/batbox/private.json", "private configuration stays put\n")
    write(home / ".batbox/private.txt", "private flight stays put\n")
    write(home / ".local/bin/batbox", "previous command\n", True)
    write(home / ".profile", "# existing login profile without final newline")
    write(home / ".bashrc", "# existing interactive profile\n")
    zdot = home / "zsh config"
    write(zdot / ".zshrc", "# existing zsh configuration\n")
    environment = dict(os.environ, HOME=str(home), SHELL=f"/bin/{shell}",
                       ZDOTDIR=str(zdot), PATH=str(shim) + os.pathsep + os.environ["PATH"])
    invoke(package, platform, environment)
    current = home / ".local/share/batbox/current"
    first_release = current.resolve()
    command = home / ".local/bin/batbox"
    assert command.is_symlink() and command.resolve() == first_release / "bin/batbox"
    assert (current / "lib/example.dylib").is_symlink()
    for name in ("LICENSE", "THIRD_PARTY_NOTICES.md", "lib/example.1.dylib",
                 "etc/ssl/certs/bundle.crt", "data/nested/asset.txt"):
        assert (current / name).read_bytes() == (package / name).read_bytes()
    actual = subprocess.check_output([str(command), "argument with spaces"], cwd=callers, text=True)
    assert Path(actual.splitlines()[0]).resolve() == callers.resolve()
    assert actual.splitlines()[1:] == ["argument with spaces"]
    backups = list((home / ".local/bin").glob("batbox-backup.*/batbox"))
    assert len(backups) == 1 and backups[0].read_text() == "previous command\n"
    invoke(package, platform, environment)
    assert current.resolve() == first_release
    assert len(list((home / ".local/share/batbox/releases").iterdir())) == 1
    profiles = [zdot / ".zprofile", zdot / ".zshrc"] if shell == "zsh" else [home / ".profile", home / ".bashrc"]
    for profile in profiles:
        assert profile.read_text().count("# >>> Batbox PATH >>>") == 1
    assert (home / ".profile").read_text().startswith("# existing login profile without final newline")
    assert (zdot / ".zshrc").read_text().startswith("# existing zsh configuration\n")
    path_check = '. "$1"; . "$2"; command -v batbox; printf "%s\\n" "$PATH"'
    path_result = subprocess.check_output(
        ["bash", "-c", path_check, "path-check", *map(str, profiles)],
        env=environment, text=True,
    ).splitlines()
    assert path_result[0] == str(command)
    assert path_result[1].split(os.pathsep).count(str(home / ".local/bin")) == 1
    if shell == "zsh" and shutil.which("zsh"):
        resolved = subprocess.check_output(["zsh", "-lic", "command -v batbox"],
                                          env=environment, text=True).strip()
        assert resolved == str(command)
    assert (home / ".config/batbox/private.json").read_text() == "private configuration stays put\n"
    assert (home / ".batbox/private.txt").read_text() == "private flight stays put\n"
    # Updates retain the old complete package and do not overwrite the source archive.
    write(package / "data/nested/asset.txt", "updated fixture data\n")
    invoke(package, platform, environment)
    assert current.resolve() != first_release
    assert (first_release / "data/nested/asset.txt").read_text() == "fixture data\n"
    assert (package / "data/nested/asset.txt").read_text() == "updated fixture data\n"
    assert len(list((home / ".local/bin").glob("batbox-backup.*"))) == 1
    # Incomplete archives fail before changing the installed command.
    last_release = current.resolve()
    (package / "THIRD_PARTY_NOTICES.md").unlink()
    invoke(package, platform, environment, success=False)
    assert current.resolve() == last_release


def source_case(temporary, platform):
    area = temporary / f"{platform} source"
    source = area / "source checkout"
    home = area / "install home"
    shim = area / "shims"
    installers(source)
    shutil.copy2(ROOT / "gadgets/package_notices.py", source / "gadgets/package_notices.py")
    write(source / "CMakeLists.txt", "fixture source\n")
    write(source / "CMakePresets.json", "{}\n")
    write(source / "LICENSE", "fixture MIT\n")
    write(source / "THIRD_PARTY_NOTICES.md", "fixture notices\n")
    write(source / "third_party/ftxui/LICENSE", "FTXUI fixture notice\n")
    write(source / "src/example/LICENSE", "embedded fixture notice\n")
    write(source / "src/example/code.cpp", "not distributed by the installer\n")
    write(source / "README.md", "public readme\n")
    write(source / "docs/BUILDING.md", "public prerequisites\n")
    write(source / "build/indigenous/batbox", "#!/bin/sh\nprintf 'fixture batbox\\n'\n", True)
    write(source / "build/private.log", "do not copy build directories\n")
    write(shim / "uname", f"#!/bin/sh\nprintf '%s\\n' {'Darwin' if platform == 'macos' else 'Linux'}\n", True)
    environment = dict(os.environ, HOME=str(home), SHELL="/bin/bash",
                       PATH=str(shim) + os.pathsep + os.environ["PATH"])
    invoke(source, platform, environment, "--skip-build", "--no-path")
    current = home / ".local/share/batbox/current"
    assert (current / "src/example/LICENSE").read_text() == "embedded fixture notice\n"
    assert not (current / "src/example/code.cpp").exists()
    assert not (current / "build").exists() and not (current / "CMakeLists.txt").exists()
    assert (current / "docs/BUILDING.md").is_file()
    assert all((current / "gadgets" / name).is_file() for name in SCRIPTS)
    assert not (home / ".bashrc").exists() and not (home / ".profile").exists()
    # Stub only the build boundary: assert each platform invokes its expected path.
    log = area / "build commands.txt"
    environment["BATBOX_FIXTURE_LOG"] = str(log)
    if platform == "macos":
        write(source / "gadgets/build-macos.sh", '#!/bin/bash\nprintf "macos build\\n" >> "$BATBOX_FIXTURE_LOG"\n', True)
    else:
        for executable in ("cmake", "ninja", "pkg-config"):
            write(shim / executable, '#!/bin/sh\nprintf "%s\\n" "$*" >> "$BATBOX_FIXTURE_LOG"\n', True)
    invoke(source, platform, environment, "--no-path")
    calls = log.read_text()
    if platform == "macos":
        assert calls == "macos build\n"
    else:
        assert "--preset indigenous\n" in calls
        assert "--build --preset indigenous --parallel" in calls


def main():
    for script in SCRIPTS:
        subprocess.run(["bash", "-n", str(ROOT / "gadgets" / script)], check=True)
    with tempfile.TemporaryDirectory(prefix="batbox install checks ") as location:
        temporary = Path(location)
        package_case(temporary, "macos", "zsh")
        package_case(temporary, "linux", "bash")
        source_case(temporary, "macos")
        source_case(temporary, "linux")
    print("POSIX install checks passed: packages, notices, source boundaries, PATH, updates, spaces, and caller cwd")


if __name__ == "__main__":
    main()
