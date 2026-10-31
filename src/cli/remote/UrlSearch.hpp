#pragma once
#include <string>

namespace batbox::cli::remote {
std::string setUrlSearchParam(const std::string& url,
  const std::string& key,const std::string& value);
}
