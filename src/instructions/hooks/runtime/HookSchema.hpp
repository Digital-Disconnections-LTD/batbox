#pragma once
#include "instructions/hooks/runtime/HookOutputParser.hpp"

namespace batbox::instructions::hooks::runtime {
struct HookSchemaImports {
  // The permission-update schema is supplied separately.
  std::function<std::optional<Json>(const Json&)> parsePermissionUpdate;
};
SchemaResult hookOutputSchemaSafeParse(const Json& input,
  const HookSchemaImports& imports);
void bindHookOutputSchema(HookParserServices& parser,HookSchemaImports imports);
} // namespace batbox::instructions::hooks::runtime
