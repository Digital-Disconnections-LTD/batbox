# Building the indigenous application

The `indigenous` preset builds the terminal executable in Release mode, with the
developer checks disabled. CMake 3.24+, Ninja, a C++20 compiler and
Git are required. FTXUI is pinned and fetched by CMake. Ada URL and cmark-gfm
also have pinned source fallbacks; an installed system cmark-gfm is preferred.
The application uses CURL, OpenSSL, ICU, libuv, libvips, yaml-cpp, PCRE2
and nlohmann-json.

The main build target and executable are `batbox` on every platform (`batbox.exe`
on Windows). Once prerequisites below are installed, `bash gadgets/install-linux.sh`,
`bash gadgets/install-macos.sh`, or PowerShell's `.\gadgets\install-windows.ps1`
builds and installs the command on your user PATH. POSIX installers accept
`--skip-build` for a binary already built in `build/indigenous`. Windows accepts
`-RuntimeRoot C:\msys64\ucrt64` when the UCRT64 prefix is not detected automatically.

The POSIX installers and `gadgets/build-macos.sh` build with six parallel jobs.
Set `BATBOX_BUILD_JOBS` to use another number, for example `BATBOX_BUILD_JOBS=2`
on a small machine.

## Homebrew (macOS)

To install a released build without a checkout, use the project tap. Homebrew
lowercases the tap owner, so the command is:

```sh
brew install digital-disconnections-ltd/batbox/batbox
```

Add `--HEAD` to build the current `main` branch instead of the latest release.
Installing by the full name trusts that one formula; to use the short name
`brew install batbox` after `brew tap digital-disconnections-ltd/batbox`, run
`brew trust --formula digital-disconnections-ltd/batbox/batbox` first (Homebrew 6
and later only load trusted formulae from non-official taps).
The formula builds from source (or pours a bottle when the tap has one), pins
FTXUI and Ada as formula resources, and takes every other library from Homebrew.
`batbox --version` reports the installed version on its first line, then the
Git commit and the date the build was configured. A build from a source archive
has no Git history and says `commit unknown` unless CMake gets
`-DBATBOX_GIT_COMMIT=<commit>`. The canonical formula lives in
`packaging/homebrew/Formula/batbox.rb`; [RELEASING.md](../RELEASING.md) explains
how a release reaches the tap.

For packagers, `cmake --install` places the executable in `bin/` and the license,
notice and documentation records in `share/doc/batbox/`. The executable reads no
data files from its prefix. Offline builds can supply pre-unpacked sources with
`-DFETCHCONTENT_SOURCE_DIR_BATBOX_FTXUI_UPSTREAM=<dir>` and
`-DBATBOX_ADA_SOURCE_DIR=<dir>`; with `-DFETCHCONTENT_FULLY_DISCONNECTED=ON` and
`-DBATBOX_FETCH_CMARK_GFM=OFF` the configure step makes no network requests.
`-DBATBOX_FORCE_BUNDLED_CMARK_GFM=ON` ignores an installed cmark-gfm and
statically links the pinned release instead.

## Linux

On Debian/Ubuntu:

```sh
sudo apt-get install cmake ninja-build g++ git pkg-config \
  nlohmann-json3-dev libcurl4-openssl-dev libssl-dev libicu-dev \
  libuv1-dev libvips-dev libyaml-cpp-dev libpcre2-dev \
  libcmark-gfm-dev libcmark-gfm-extensions-dev
cmake --preset indigenous
cmake --build --preset indigenous --parallel 6
./build/indigenous/batbox --help
```

Ripgrep, Git, and a shell support the coding gadgets. PDF helpers use Poppler and
Ghostscript. Web gadgets use a SearXNG endpoint and a Scrapling executable; their
private settings and credentials are supplied separately from the build.

## macOS

Use Xcode 26.2 or newer, including its macOS SDK: the C++ library must provide
`std::stop_token` and `std::jthread`. CI selects Xcode 26.2 on both architectures
and checks those features before building. If several Xcode versions are installed,
select one for your shell with `export DEVELOPER_DIR=/Applications/Xcode_26.2.app/Contents/Developer`
and `export SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"`.

The indigenous build helper configures the current Intel or Apple Silicon machine,
including Homebrew library prefixes and the AppKit clipboard adapter:

```sh
brew install cmake ninja pkgconf nlohmann-json curl openssl@3 icu4c \
  libuv vips yaml-cpp pcre2 cmark-gfm git ripgrep
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
  mingw-w64-ucrt-x86_64-pcre2
cmake --preset indigenous
cmake --build --preset indigenous --parallel 6
./build/indigenous/batbox.exe --help
```

CMake fetches pinned cmark-gfm when the system package is absent. Run the current
build inside its MSYS2 environment so dependency DLLs are on PATH. A relocatable DLL ZIP can be created with
`gadgets/package_indigenous.py --runtime-root /ucrt64`; see
[indigenous-windows.md](indigenous-windows.md). Indigenous interactive Windows acceptance
remains separate work. MSYS2 documents the
[UCRT64 environment](https://www.msys2.org/docs/environments/)
and [libvips package](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-libvips).

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
persistence. The fixture uses its own temporary configuration. Scrapling is not
needed: Batbox starts it only when WebFetch first runs. Pass
`--scrapling-fixture build/indigenous-checks/batbox-scrapling-fixture` to point
`scrapling_command` at the offline helper built by the checks preset.

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
clipboard, browser authentication, and credential storage.
