# Third-party notices

Batbox’s project-owned code is distributed under [MIT](LICENSE). Third-party
components and adapted algorithms retain their own licenses and copyright notices.
The project license does not replace those terms.

| Component or material | License and notices |
| --- | --- |
| FTXUI terminal interface | [MIT](third_party/ftxui/LICENSE) |
| Ada URL | [MIT](third_party/licenses/ada-LICENSE-MIT.txt) / [Apache-2.0](third_party/licenses/ada-LICENSE-APACHE.txt) |
| cmark-gfm | [Copyright and license notices](third_party/licenses/cmark-gfm-COPYING.txt) |
| Anthropic SDK algorithm adaptations | [Apache-2.0](third_party/notices/third-party/sdk/LICENSE), [NOTICE](third_party/notices/third-party/sdk/NOTICE) |
| ANSI parsing and width data | Notices alongside [ANSI sources](src/ansi_tokenize/) and [wrapping sources](src/npm_wrap_ansi/licenses/) |
| Unicode width tables | [Unicode license](src/bun_text_width/UNICODE-LICENSE.upstream.txt) and [Bun notices](src/bun_text_width/LICENSE.upstream.md) |
| AWS SigV4 signing test vectors (test fixtures only) | [Apache-2.0](tests/fixtures/sigv4/LICENSE), [NOTICE](tests/fixtures/sigv4/NOTICE), from [aws-c-auth revision 3d29adcecf5c032131acee9f4125a636d2aaea6a](https://github.com/awslabs/aws-c-auth/tree/3d29adcecf5c032131acee9f4125a636d2aaea6a/tests/aws-signing-test-suite/v4) |
| Additional adapted helpers | Preserved notices in [third_party/notices](third_party/notices/) and [third_party/licenses](third_party/licenses/) |

SDK adaptations in `src/transport/` use algorithms from
[Anthropic’s SDK revision cb5fba419808e3844fa6407e59577ab7ba346b79](https://github.com/anthropics/anthropic-sdk-typescript/tree/cb5fba419808e3844fa6407e59577ab7ba346b79).
Copyright 2023 Anthropic, PBC. Batbox’s C++ adaptations and integration changes
remain subject to the SDK’s Apache-2.0 terms.

System-linked dependencies include CURL, OpenSSL, ICU, libuv, libvips, yaml-cpp,
PCRE2 and nlohmann-json. Their package licenses apply. Distribution
of runtime binaries also requires the corresponding package notices and any
applicable source/relinking obligations. Packaging records the runtime inventory;
review the exact bundled packages before a public binary release.

[Scrapling](https://github.com/D4Vinci/Scrapling) and
[SearXNG](https://github.com/searxng/searxng) are independently installed services
with their own licenses. They are not vendored into this source tree.
