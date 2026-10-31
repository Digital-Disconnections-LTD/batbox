add_library(batbox_hook_schema src/instructions/hooks/runtime/HookResponseSchema.cpp)
set(batbox_json_schema_src third_party/json-schema-validator/src)
add_library(batbox_json_schema_vendor
  ${batbox_json_schema_src}/json-uri.cpp ${batbox_json_schema_src}/json-patch.cpp
  ${batbox_json_schema_src}/json-validator.cpp ${batbox_json_schema_src}/json-schema-draft7.json.cpp
  ${batbox_json_schema_src}/string-format-check.cpp ${batbox_json_schema_src}/smtp-address-validator.cpp)
target_include_directories(batbox_json_schema_vendor PUBLIC ${batbox_json_schema_src})
target_link_libraries(batbox_json_schema_vendor PUBLIC nlohmann_json::nlohmann_json)
target_sources(batbox_hook_schema PRIVATE src/gadgets/synthetic_output/IndigenousJsonSchemaBackend.cpp
  src/gadgets/synthetic_output/IndigenousAjvDiagnostics.cpp src/gadgets/synthetic_output/IndigenousStrictTypes.cpp)
target_link_libraries(batbox_hook_schema PUBLIC batbox_core PRIVATE batbox_json_schema_vendor)
target_compile_options(batbox_hook_schema PRIVATE -Wall -Wextra -Wpedantic)
add_library(batbox_synthetic_output src/gadgets/synthetic_output/SyntheticOutput.cpp
  src/gadgets/synthetic_output/SyntheticOutputFactory.cpp)
target_link_libraries(batbox_synthetic_output PUBLIC batbox_core batbox_hook_schema)
target_compile_options(batbox_synthetic_output PRIVATE -Wall -Wextra -Wpedantic)
