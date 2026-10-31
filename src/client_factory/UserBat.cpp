#include "client_factory/UserBat.hpp"
#include <stdexcept>

namespace batbox::client_factory {
std::string getBatboxUserBat(const std::string& version){
  if(version.empty())throw std::invalid_argument(
    "Batbox version is required for User-Agent");
  return "batbox-cli/"+version;
}
}
