#pragma once
#include "runtime/JsonValue.hpp"

namespace batbox::application_bats {
// Built-in general-purpose Bat definition and poetry. Environment-specific
// additions come from the shared enhanceSystemPoetryWithEnvDetails provider.
std::string generalPurposePoetry();
Json generalPurposeDefinition();
} // namespace batbox::application_bats
