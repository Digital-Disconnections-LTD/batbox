#pragma once
#include "application_file_history/History.hpp"
#include <string_view>

namespace batbox::application_file_history::detail {
Json emptyState();
std::vector<std::string> tracked(const Json& state);
Json snapshots(const Json& state);
std::string stringField(const Json& value,std::string_view key);
std::size_t version(const Json& backup);
bool envTruthy(const char* name);
std::optional<std::string> readOrNull(const std::filesystem::path& file);
} // namespace batbox::application_file_history::detail
