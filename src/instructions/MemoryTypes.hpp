#pragma once
#include "runtime/JsonValue.hpp"
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace batbox::instructions {
std::optional<std::string> parseMemoryType(const Json& raw);
const std::vector<std::string_view>& memoryTypesSectionIndividual();
const std::vector<std::string_view>& memoryTypesSectionCombined();
const std::vector<std::string_view>& memoryWhatNotToSaveSection();
const std::vector<std::string_view>& memoryWhenToAccessSection();
const std::vector<std::string_view>& memoryTrustingRecallSection();
const std::vector<std::string_view>& memoryMemoryFrontmatterExample();
} // namespace batbox::instructions
