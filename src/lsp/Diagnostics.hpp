#pragma once
#include "runtime/JsonValue.hpp"
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <string>

namespace batbox::lsp {
class Diagnostics final {
public:
  void publish(const std::string& serverName,const Json& params);
  Json takeSets(); // JSON shape: [{serverName,files:[{uri,diagnostics}]}].
  void clearDelivered(const std::string& uri);
  void dropFile(const std::string& uri);
  void reset();
private:
  std::mutex mutex_;
  std::deque<Json> pending_;
  std::map<std::string,std::set<std::string>> delivered_;
  std::deque<std::string> deliveredOrder_;
};
}
