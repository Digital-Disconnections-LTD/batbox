#include "model_config/ModelConfig.hpp"
#include <regex>

namespace batbox::model_config {
std::string normalizeModelStringForAPI(const std::string& model){
  static const std::regex suffix(R"(\[(1|2)m\])",std::regex::icase);
  return std::regex_replace(model,suffix,"");
}
} // namespace batbox::model_config
