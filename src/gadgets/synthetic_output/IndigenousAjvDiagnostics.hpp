#pragma once
#include "gadgets/synthetic_output/SyntheticOutputFactory.hpp"

namespace batbox::gadgets::synthetic_output {
std::string renderIndigenousIssueAsAjv(const Json& schema,const Json& value,
  const AjvValidationIssue& issue);
std::optional<std::string> ajvStrictFormatError(const Json& schema);
std::optional<std::string> ajvMetaErrorText(const Json& schema,
  const std::string& indigenousText);
} // namespace batbox::gadgets::synthetic_output
