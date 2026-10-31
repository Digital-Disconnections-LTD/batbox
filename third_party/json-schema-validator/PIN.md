# Native JSON Schema validation dependency

- Upstream: https://github.com/pboettch/json-schema-validator
- Release: `2.4.0`
- Commit: `c780404a84dd9ba978ba26bc58d17cb43fa7bc80`
- License: MIT, preserved in `LICENSE`.
- Included files: upstream `src/` only, for static native compilation.

This is an external dependency used by the native SyntheticOutputTool schema adapter. The private source archive does not include a native JSON Schema validator. Upstream supports Draft 7 but its strict schema rules and error text differ from Ajv 8; the adapter accounts for tested differences and records remaining limits separately.
