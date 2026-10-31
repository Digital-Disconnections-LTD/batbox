# Building the indigenous application

The `indigenous` preset builds the terminal executable in Release mode, with the
developer checks disabled. CMake 3.24+, Ninja, a C++20 compiler and
Git are required. FTXUI is pinned and fetched by CMake. Ada URL and cmark-gfm
also have pinned source fallbacks; an installed system cmark-gfm is preferred.
The application uses CURL, OpenSSL, ICU, libuv, libvips, yaml-cpp, PCRE2,
nlohmann-json and libwebsockets.

The main build target and executable are `batbox` on every platform (`batbox.exe`
on Windows). Once prerequisites below are installed, `bash gadgets/install-linux.sh`,
`bash gadgets/install-macos.sh`, or PowerShell's `.\gadgets\install-windows.ps1`
builds and installs the command on your user PATH. POSIX installers accept
`--skip-build` for a binary already built in `build/indigenous`. Windows accepts
`-RuntimeRoot C:\msys64\ucrt64` when the UCRT64 prefix is not detected automatically.

## Linux

On Debian/Ubuntu:

```sh
sudo apt-get install cmake ninja-build g++ git pkg-config \
  nlohmann-json3-dev libcurl4-openssl-dev libssl-dev libicu-dev \
  libuv1-dev libvips-dev libyaml-cpp-dev libpcre2-dev libwebsockets-dev \
  libcmark-gfm-dev libcmark-gfm-extensions-dev
cmake --preset indigenous
cmake --build --preset indigenous --parallel 6
./build/indigenous/batbox --help
```

Ripgrep, Git, and a shell support the coding gadgets. PDF helpers use Poppler and
Ghostscript. Web gadgets use a SearXNG endpoint and a Scrapling executable; their
private settings and credentials are supplied separately from the build.

## macOS

The indigenous build helper configures the current Intel or Apple Silicon machine,
including Homebrew library prefixes and the AppKit clipboard adapter:

```sh
brew install cmake ninja pkgconf nlohmann-json curl openssl@3 icu4c \
  libuv vips yaml-cpp pcre2 libwebsockets cmark-gfm git ripgrep
bash gadgets/build-macos.sh
./build/indigenous/batbox --help
```

See [indigenous-macos.md](indigenous-macos.md) for architecture-specific packaging and
remaining indigenous acceptance. The macOS package currently depends on installed
Homebrew libraries; it is not a standalone installer.

## Windows

The intended indigenous compiler environment is MSYS2 UCRT64 with GCC and Ninja.
Use the UCRT64 shell for dependency and build commands, and install Git for Windows for the Bash Gadget.

```sh
pacman -S --needed git mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-nlohmann-json \
  mingw-w64-ucrt-x86_64-curl mingw-w64-ucrt-x86_64-openssl \
  mingw-w64-ucrt-x86_64-icu mingw-w64-ucrt-x86_64-libuv \
  mingw-w64-ucrt-x86_64-libvips mingw-w64-ucrt-x86_64-yaml-cpp \
  mingw-w64-ucrt-x86_64-pcre2 mingw-w64-ucrt-x86_64-libwebsockets
cmake --preset indigenous
cmake --build --preset indigenous --parallel 6
./build/indigenous/batbox.exe --help
```

CMake fetches pinned cmark-gfm when the system package is absent. Run the current
build inside its MSYS2 environment so dependency DLLs are on PATH. A relocatable DLL ZIP can be created with
`gadgets/package_indigenous.py --runtime-root /ucrt64`; see
[indigenous-windows.md](indigenous-windows.md). Indigenous interactive Windows acceptance
remains separate work. MSYS2 documents the
[UCRT64 environment](https://www.msys2.org/docs/environments/),
[libvips package](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-libvips)
and [libwebsockets package](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-libwebsockets).

## Developer checks

The `indigenous-checks` preset enables C++ fixtures and Python integration checks.
No JavaScript runtime or dependency installation is needed.
Install Poppler and Ghostscript for the PDF integration checks (`brew install
poppler ghostscript` on macOS, or `sudo apt-get install poppler-utils ghostscript`
on Debian/Ubuntu).

```sh
cmake --preset indigenous-checks
cmake --build build/indigenous-checks --parallel 6
ctest --test-dir build/indigenous-checks --output-on-failure
```

For a focused change, build the affected `batbox-*` test targets and select their
CTest names with `-R`. Assertions remain enabled in Release test executables.

The local Messages fixture needs no model account:

```sh
python3 checks/indigenous_platform_smoke.py build/indigenous/batbox
python3 checks/indigenous_platform_smoke.py build/indigenous/batbox --skill-shell
python3 checks/indigenous_platform_smoke.py build/indigenous/batbox --mission-board
```

It checks streaming, Unicode file paths, file reading, shell expansion, and Mission
persistence. The fixture uses its own temporary configuration. Pass
`--scrapling-fixture build/indigenous-checks/batbox-scrapling-fixture` to use the
offline helper built by the checks preset; otherwise Scrapling must be on PATH.

## Packaging

```sh
python3 gadgets/package_macos.py --binary build/indigenous/batbox --output dist/batbox-macos.tar.gz
python3 gadgets/package_indigenous.py --binary build/indigenous/batbox --output dist/batbox-linux.tar.gz
```

Run the macOS command on macOS and the Linux command on Linux. macOS packages
record their host Homebrew library dependencies in `MACHO-LINKS.json`; they do not
yet bundle dylibs, sign, or notarize an installer. Linux packages record `ldd`
output and require compatible system libraries.

From a coherent MSYS2 UCRT64 environment:

```sh
python gadgets/package_indigenous.py --binary build/indigenous/batbox.exe --runtime-root /ucrt64 --output dist/batbox-windows.zip
```

The Windows packager follows imported DLLs recursively and includes the generated
CA bundle. It rejects missing DLLs or an empty certificate bundle. Optional media
loaders, PDF helpers, Git, ripgrep, and Scrapling remain external. All packagers
copy project and dependency notices and record hashes in `BUILD.json`.

Windows packaging also preserves the runtime prefix's installed `share/licenses`
records, supplemental notice files from `share/doc` and `share/icu`, and any
MSYS2 `usr/share/licenses/common` records. A missing or empty `share/licenses`
tree, empty records, or missing referenced common texts stop packaging.
`licenses/windows-runtime/INVENTORY.json` records
their paths and hashes. This bounded collection may include unbundled packages;
review it against the exact DLL/package inventory before distribution. Package
records alone do not settle missing license text or source/relinking requirements.
Run `python3 checks/indigenous_windows_package.py` to check this packaging boundary.

## Platform checks

`.github/workflows/indigenous-platforms.yml` retains Linux, Apple Silicon macOS,
Intel macOS, and UCRT64 Windows builds. It runs on pull requests, main pushes,
and manual dispatch. Artifacts are preview packages. A configured workflow is not
a claim that hosted jobs have run.

Before release, test the packages on clean machines and exercise real terminals,
clipboard, browser authentication, and credential storage. See [the roadmap](../ROADMAP.md).
