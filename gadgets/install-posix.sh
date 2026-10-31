#!/bin/bash
set -euo pipefail

platform=${1:-}
shift || true
skip_build=false
setup_path=true
usage() {
  echo "Usage: gadgets/install-$platform.sh [--skip-build] [--no-path]"
  echo 'Build a source checkout, or install the complete extracted release package.'
  echo 'Installs under ~/.local; never installs dependencies or changes private config.'
}
fail() { echo "Batbox install: $*" >&2; exit 2; }
for argument in "$@"; do
  case "$argument" in
    --skip-build) skip_build=true ;;
    --no-path) setup_path=false ;;
    --help|-h) usage; exit 0 ;;
    *) usage >&2; fail "Unknown option: $argument" ;;
  esac
done
case "$platform:$(uname -s)" in
  macos:Darwin|linux:Linux) ;;
  *) fail "Use the installer for this operating system (macos or linux)." ;;
esac
[[ -n ${HOME:-} && $HOME = /* ]] || fail 'HOME must name an absolute user directory.'
installer_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
input_root=$(cd "$installer_dir/.." && pwd)
bin_dir="$HOME/.local/bin"
package_root="$HOME/.local/share/batbox"
command_path="$bin_dir/batbox"
current_path="$package_root/current"
payload=''
cleanup() {
  # Only an unpublished directory created by this invocation is removed.
  if [[ -n $payload && -d $payload ]]; then rm -rf -- "$payload"; fi
}
trap cleanup EXIT

[[ -f "$input_root/LICENSE" && -f "$input_root/THIRD_PARTY_NOTICES.md" ]] ||
  fail 'Package is missing LICENSE or THIRD_PARTY_NOTICES.md; extract the whole archive.'
if [[ -e $current_path && ! -L $current_path ]]; then
  fail "$current_path already exists and is not an installer symlink; it was left untouched."
fi
if [[ -d $command_path && ! -L $command_path ]]; then
  fail "$command_path is a directory; it was left untouched."
fi

if [[ -f "$input_root/CMakeLists.txt" && -f "$input_root/CMakePresets.json" ]]; then
  for executable in python3; do
    command -v "$executable" >/dev/null 2>&1 ||
      fail "Missing $executable. See docs/BUILDING.md for platform prerequisites."
  done
  [[ -f "$input_root/third_party/ftxui/LICENSE" ]] ||
    fail 'Missing FTXUI license. Use a complete source checkout; see docs/BUILDING.md.'
  if ! $skip_build; then
    if [[ $platform = macos ]]; then
      bash "$input_root/gadgets/build-macos.sh"
    else
      for executable in cmake ninja pkg-config; do
        command -v "$executable" >/dev/null 2>&1 ||
          fail "Missing $executable. See docs/BUILDING.md for Linux build prerequisites."
      done
      (
        cd "$input_root"
        cmake --preset indigenous
        cmake --build --preset indigenous --parallel "${BATBOX_BUILD_JOBS:-6}"
      )
    fi
  fi
  binary="$input_root/build/indigenous/batbox"
  [[ -f $binary ]] || fail "Missing $binary. Run the installer without --skip-build to build it."
  mkdir -p "$package_root/releases"
  payload=$(mktemp -d "$package_root/releases/release.XXXXXXXX")
  mkdir -p "$payload/bin"
  cp -p "$binary" "$payload/bin/batbox"
  python3 - "$input_root" "$payload" <<'PY'
import sys
from pathlib import Path
source, destination = map(Path, sys.argv[1:])
sys.path.insert(0, str(source / "gadgets"))
from package_notices import stage_notices
stage_notices(source, destination)
PY
  if [[ -f "$input_root/README.md" ]]; then cp -p "$input_root/README.md" "$payload/"; fi
  if [[ -d "$input_root/docs" ]]; then cp -R "$input_root/docs" "$payload/docs"; fi
  mkdir -p "$payload/gadgets"
  for script in install-macos.sh install-linux.sh install-posix.sh; do
    cp -p "$installer_dir/$script" "$payload/gadgets/$script"
  done
else
  [[ -f "$input_root/bin/batbox" ]] ||
    fail 'No bin/batbox found. Run this installer inside a complete extracted package or source checkout.'
  mkdir -p "$package_root/releases"
  payload=$(mktemp -d "$package_root/releases/release.XXXXXXXX")
  # Keep bundled libraries, data, certificates and notices beside the binary.
  cp -R "$input_root/." "$payload/"
fi
chmod u+x "$payload/bin/batbox"
mkdir -p "$bin_dir"
if [[ -e $command_path || -L $command_path ]]; then
  if [[ ! -L $command_path || $(readlink "$command_path") != "$current_path/bin/batbox" ]]; then
    backup=$(mktemp -d "$bin_dir/batbox-backup.XXXXXXXX")
    mv "$command_path" "$backup/batbox"
    echo "Previous command preserved at: $backup/batbox"
  fi
fi
if [[ -L $current_path && -d $current_path ]] &&
    diff -qr "$payload" "$current_path" >/dev/null; then
  rm -rf -- "$payload"
  payload=''
else
  ln -sfn "$payload" "$current_path"
  payload=''
fi
ln -sfn "$current_path/bin/batbox" "$command_path"

append_path() {
  local profile=$1
  local marker='# >>> Batbox PATH >>>'
  if [[ -f $profile ]] && grep -Fqx "$marker" "$profile"; then return; fi
  mkdir -p "$(dirname "$profile")"
  cat >> "$profile" <<'SH'

# >>> Batbox PATH >>>
case ":$PATH:" in
  *":$HOME/.local/bin:"*) ;;
  *) export PATH="$HOME/.local/bin:$PATH" ;;
esac
# <<< Batbox PATH <<<
SH
  echo "PATH setup added to: $profile"
}
if $setup_path; then
  shell_name=${SHELL:-}
  shell_name=${shell_name##*/}
  case "$shell_name" in
    zsh)
      append_path "${ZDOTDIR:-$HOME}/.zprofile"
      append_path "${ZDOTDIR:-$HOME}/.zshrc"
      ;;
    bash|sh|'')
      append_path "$HOME/.bashrc"
      if [[ -f "$HOME/.bash_profile" ]]; then append_path "$HOME/.bash_profile"
      elif [[ -f "$HOME/.bash_login" ]]; then append_path "$HOME/.bash_login"
      else append_path "$HOME/.profile"; fi
      ;;
    *) echo "Add $bin_dir to your shell's PATH to use the batbox command." ;;
  esac
fi
echo "Installed: $command_path"
echo 'Open a new terminal, then run: batbox'
echo 'For this terminal, run: export PATH="$HOME/.local/bin:$PATH"'
