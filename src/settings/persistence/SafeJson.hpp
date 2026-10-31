#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace batbox::settings::persistence {
class SafeJsonParser {
 public:
  using Logger=std::function<void(const std::string&)>;
  explicit SafeJsonParser(Logger logError);
  Json safeParseJSON(const std::optional<std::string>& text,
    bool shouldLogError=true);
  void clear();
  std::size_t stashSize() const;
 private:
  struct Entry {std::string key;Json value;};
  Json parseUncached(const std::string& text,bool shouldLogError);
  Logger logError_;
  mutable std::mutex mutex_;
  std::list<Entry> entries_;
  std::unordered_map<std::string,std::list<Entry>::iterator> index_;
};
}
