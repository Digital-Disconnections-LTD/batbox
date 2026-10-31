#pragma once
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace batbox::auth::oauth_client {
using QueryEntries=std::vector<std::pair<std::string,std::string>>;
QueryEntries parseFormQuery(std::string_view query);
std::string serializeFormQuery(const QueryEntries& entries);
std::string appendFormQuery(std::string_view href,
  const QueryEntries& entries);
}
