# Releasing Batbox

A release is a version tag on `main`, a GitHub release for it, and a formula
update in the Homebrew tap
([Digital-Disconnections-LTD/homebrew-batbox](https://github.com/Digital-Disconnections-LTD/homebrew-batbox)).

1. **Bump the version.** Set `project(batbox VERSION x.y.z ...)` in
   `CMakeLists.txt`. The welcome screen shows it, so update the version text in
   `tests/phase6/goldens/visual/welcome-*.txt` too, then build and run the
   checks:

   ```sh
   cmake --build build/indigenous-checks -j8
   ctest --test-dir build/indigenous-checks -j8
   ./build/indigenous-checks/batbox --version   # batbox x.y.z
   ```

   Merge the bump to `main`.

2. **Tag and release.** From the merged commit:

   ```sh
   git tag -a vX.Y.Z -m "Batbox X.Y.Z"
   git push origin vX.Y.Z
   gh release create vX.Y.Z --generate-notes
   ```

   The tag starts the platform build workflow. When every platform is green
   (about an hour and a half), its release job attaches the packages and their
   checksums to the release, creating it first if you have not:
   `batbox-X.Y.Z-linux-x86_64.tar.gz`, `batbox-X.Y.Z-macos-arm64.tar.gz`,
   `batbox-X.Y.Z-macos-x86_64.tar.gz`, `batbox-X.Y.Z-windows-x86_64.zip` and
   `SHA256SUMS`. Nothing to upload by hand; if a flaky check fails, re-run the
   failed jobs and the release job follows.

3. **Hash the source tarball.** GitHub serves the tag archive the formula uses:

   ```sh
   curl -fsSL https://github.com/Digital-Disconnections-LTD/batbox/archive/refs/tags/vX.Y.Z.tar.gz | shasum -a 256
   ```

4. **Update the formula.** This is the one hand step left. In
   `packaging/homebrew/Formula/batbox.rb`, set `url` to the new tag and
   `sha256` to the hash. If `cmake/FtxuiDependency.cmake` or
   `cmake/IndigenousDependencies.cmake` changed their pinned FTXUI or Ada
   archives, update the matching `resource` blocks (same URL and SHA-256). Check
   it with `brew style` and `brew audit --strict --online batbox` from the tap.

5. **Push the tap.** Copy the formula into the tap repository's `Formula/` on a
   branch and open a pull request. The tap's daily `brew bump` workflow may open
   that pull request for you after the tag appears.

6. **Bottles.** The tap's `brew test-bot` workflow builds and tests the formula on
   every pull request (macOS and Linux runners) and uploads the bottles as an
   artifact. When it is green, run the tap's `brew pr-pull` workflow (Actions tab,
   or `gh workflow run publish.yml -R Digital-Disconnections-LTD/homebrew-batbox
   -f pull_request=<number>`). It uploads the bottles to the GitHub release of
   the tap, adds the `bottle do` block to the formula and pushes to `main`. Users
   then pour the bottle instead of compiling.

Copy the bottled formula back into `packaging/homebrew/Formula/batbox.rb` so the
repository keeps the canonical version in step with the tap.
