#!/bin/bash
set -euo pipefail

if [[ $(uname -s) != Darwin ]]; then
  echo 'Run this build on macOS with Xcode Command Line Gadgets.' >&2
  exit 2
fi
if [[ ${1:-} != '' && ${1:-} != '--configure-only' ]]; then
  echo 'Usage: gadgets/build-macos.sh [--configure-only]' >&2
  exit 2
fi
for executable in brew cmake ninja pkg-config xcrun; do
  if ! command -v "$executable" >/dev/null 2>&1; then
    echo "Missing build command: $executable" >&2
    exit 2
  fi
done
xcrun --sdk macosx --show-sdk-path >/dev/null

case $(uname -m) in
  arm64|x86_64) build_arch=$(uname -m) ;;
  *) echo 'A indigenous arm64 or x86_64 macOS host is required.' >&2; exit 2 ;;
esac

formulae=(nlohmann-json curl openssl@3 icu4c libuv vips yaml-cpp
  pcre2 libwebsockets cmark-gfm)
prefixes=()
pkgpaths=()
for formula in "${formulae[@]}"; do
  if ! brew list --versions "$formula" >/dev/null 2>&1; then
    echo "Missing Homebrew formula: $formula" >&2
    exit 2
  fi
  prefix=$(brew --prefix "$formula")
  prefixes+=("$prefix")
  if [[ -d $prefix/lib/pkgconfig ]]; then
    pkgpaths+=("$prefix/lib/pkgconfig")
  fi
done
if [[ -n ${CMAKE_PREFIX_PATH:-} ]]; then prefixes+=("$CMAKE_PREFIX_PATH"); fi
if [[ -n ${PKG_CONFIG_PATH:-} ]]; then pkgpaths+=("$PKG_CONFIG_PATH"); fi
export CMAKE_PREFIX_PATH=$(IFS=:; echo "${prefixes[*]}")
export PKG_CONFIG_PATH=$(IFS=:; echo "${pkgpaths[*]}")

source_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$source_dir"
cmake --preset indigenous -DCMAKE_OSX_ARCHITECTURES="$build_arch"
if [[ ${1:-} != '--configure-only' ]]; then
  cmake --build --preset indigenous --parallel "${BATBOX_BUILD_JOBS:-6}"
fi
