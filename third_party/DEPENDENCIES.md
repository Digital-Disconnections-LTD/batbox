# Dependency SDK dependencies

The C++ application links system libraries for networking, text, images,
cryptography and configuration. Their upstream package licenses apply.

Pinned fetched dependencies are:

| Library | Version or revision | License record |
| --- | --- | --- |
| FTXUI | f921fad208912747c17d129a8ef75ec7624b6eec | [MIT license](ftxui/LICENSE) |
| Ada URL | 4.0.0 | [MIT](licenses/ada-LICENSE-MIT.txt) or [Apache-2.0](licenses/ada-LICENSE-APACHE.txt) |
| cmark-gfm fallback | 0.29.0.gfm.13 | [upstream notices](licenses/cmark-gfm-COPYING.txt) |

The CMake declarations verify release archive SHA-256 values. Offline source
checkouts can be supplied through the corresponding FetchContent source-directory
variables.
See [the complete notices](../THIRD_PARTY_NOTICES.md).
