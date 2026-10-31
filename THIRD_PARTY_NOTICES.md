# Third-party notices

Batbox’s project-owned code is distributed under [MIT](LICENSE). Third-party
components and adapted algorithms retain their own licenses and copyright notices.
The project license does not replace those terms.

| Component or material | License and notices |
| --- | --- |
| FTXUI terminal interface | [MIT](third_party/ftxui/LICENSE) |
| Ada URL | [MIT](third_party/licenses/ada-LICENSE-MIT.txt) / [Apache-2.0](third_party/licenses/ada-LICENSE-APACHE.txt) |
| cmark-gfm | [Copyright and license notices](third_party/licenses/cmark-gfm-COPYING.txt) |
| JSON Schema validator | [MIT](third_party/json-schema-validator/LICENSE) |
| Anthropic SDK algorithm adaptations | [Apache-2.0](third_party/notices/third-party/sdk/LICENSE), [NOTICE](third_party/notices/third-party/sdk/NOTICE) |
| ANSI parsing and width data | Notices alongside [ANSI sources](src/ansi_tokenize/) and [wrapping sources](src/npm_wrap_ansi/licenses/) |
| Unicode width tables | [Unicode license](src/bun_text_width/UNICODE-LICENSE.upstream.txt) and [Bun notices](src/bun_text_width/LICENSE.upstream.md) |
| Bidirectional text | [bidi-js notice](src/terminal_output/LICENSE.bidi-js.txt) |
| Throttling | [Lodash MIT](src/lodash_throttle/LICENSE.lodash-es) |
| Terminal colors and rendering data | [Chalk](src/terminal_color/LICENSE.chalk.txt), [cli-boxes](src/terminal_render/LICENSE.cli-boxes.txt), [indent-string](src/terminal_render/LICENSE.indent-string.txt) |
| Additional adapted helpers | Preserved notices in [third_party/notices](third_party/notices/) and [third_party/licenses](third_party/licenses/) |

SDK adaptations in `src/transport/` use algorithms from
[Anthropic’s SDK revision cb5fba419808e3844fa6407e59577ab7ba346b79](https://github.com/anthropics/anthropic-sdk-typescript/tree/cb5fba419808e3844fa6407e59577ab7ba346b79).
Copyright 2023 Anthropic, PBC. Batbox’s C++ adaptations and integration changes
remain subject to the SDK’s Apache-2.0 terms.

System-linked dependencies include CURL, OpenSSL, ICU, libuv, libvips, yaml-cpp,
PCRE2, nlohmann-json, and libwebsockets. Their package licenses apply. Distribution
of runtime binaries also requires the corresponding package notices and any
applicable source/relinking obligations. Packaging records the runtime inventory;
review the exact bundled packages before a public binary release.

[Scrapling](https://github.com/D4Vinci/Scrapling) and
[SearXNG](https://github.com/searxng/searxng) are independently installed services
with their own licenses. They are not vendored into this source tree.
