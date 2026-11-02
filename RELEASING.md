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

   The tag starts the platform build workflow. When the Linux and macOS builds
   are green (about an hour and a half), its release job attaches the packages
   and their checksums to the release, creating it first if you have not:
   `batbox-X.Y.Z-linux-x86_64.tar.gz`, `batbox-X.Y.Z-macos-arm64.tar.gz`,
   `batbox-X.Y.Z-macos-x86_64.tar.gz`, `batbox-X.Y.Z-windows-x86_64.zip` and
   `SHA256SUMS`. The Windows zip is attached only when the Windows build passes;
   the job warns and carries on without it. Nothing to upload by hand; if a
   flaky check fails, re-run the failed jobs and the release job follows.

3. **Bump the tap.** After the GitHub release is published, start the tap's
   `brew bump` workflow (or wait for its daily run at 19:40 UTC):

   ```sh
   gh workflow run autobump.yml --repo Digital-Disconnections-LTD/homebrew-batbox
   ```

   It finds the new release through the formula's `livecheck` block and opens a
   pull request on the tap with the new `url` and `sha256`. That pull request
   starts test-bot, which builds the bottles. `brew bump` does not touch the
   `resource` blocks, so if `cmake/FtxuiDependency.cmake` or
   `cmake/IndigenousDependencies.cmake` changed their pinned FTXUI or Ada
   archives, push the matching `resource` updates (same URL and SHA-256) to the
   bump branch. Then copy the new `url`, `sha256` and any `resource` changes
   into `packaging/homebrew/Formula/batbox.rb` on `main`.

   The workflow opens the pull request with the tap secret
   `HOMEBREW_GITHUB_API_TOKEN`, a fine-grained personal access token scoped to
   the tap repository, because the Actions token cannot open pull requests
   there and its pull requests would not start test-bot. The token expires on a
   date the owner set, and GitHub emails before it does. To renew it, regenerate
   the token on its GitHub settings page with a new expiry, then run
   `gh secret set HOMEBREW_GITHUB_API_TOKEN --repo Digital-Disconnections-LTD/homebrew-batbox`
   and paste the new value. An expired token shows up as an autobump run that
   fails at the pull request step with a 401; until it is renewed, pin the
   formula by hand (below).

4. **Wait for test-bot.** The tap's `brew test-bot` workflow builds and tests the
   formula on the bump pull request, as on every pull request that changes it
   (macOS 26 arm64 and Linux x86_64 runners), and uploads the bottles as
   artifacts. Other platforms, such as Intel Macs and older macOS releases,
   still build from source. Fix any failure on the branch until both jobs are
   green:

   ```sh
   gh pr checks <number> --watch -R Digital-Disconnections-LTD/homebrew-batbox
   ```

5. **Publish the bottles.** The tap's `brew pr-pull` workflow is started by hand,
   not by a label (current Homebrew tap template). Once test-bot is green, run
   it from the Actions tab or with:

   ```sh
   gh workflow run publish.yml -R Digital-Disconnections-LTD/homebrew-batbox -f pull_request=<number>
   ```

   It uploads the bottles to a `batbox-X.Y.Z` release on the tap repository,
   commits a `bottle do` block to the formula, pushes to the tap's `main` and
   closes the pull request. Users then pour the bottle instead of compiling;
   check with `brew update && brew reinstall digital-disconnections-ltd/batbox/batbox`,
   which should say `Pouring batbox-X.Y.Z.<platform>.bottle...tar.gz`.

The `bottle do` block lives only in the tap. The publish job rewrites it on every
release, so do not copy it into `packaging/homebrew/Formula/batbox.rb`; apart from
that block the two formulas stay identical.

## Pinning the formula by hand

Use this when the bump workflow cannot run, for example while its token is
expired, then continue from step 4.

1. **Hash the source tarball.** GitHub serves the tag archive the formula uses:

   ```sh
   curl -fsSL https://github.com/Digital-Disconnections-LTD/batbox/archive/refs/tags/vX.Y.Z.tar.gz | shasum -a 256
   ```

2. **Update the formula.** In `packaging/homebrew/Formula/batbox.rb`, set `url`
   to the new tag and `sha256` to the hash. If `cmake/FtxuiDependency.cmake` or
   `cmake/IndigenousDependencies.cmake` changed their pinned FTXUI or Ada
   archives, update the matching `resource` blocks (same URL and SHA-256). Check
   it with `brew style` and `brew audit --strict --online batbox` from the tap.

3. **Push the tap.** Copy the formula into the tap repository's `Formula/` on a
   branch, keeping the tap's `bottle do` block, and open a pull request against
   `main`. `brew livecheck batbox` shows whether the tap is behind the latest
   release.
