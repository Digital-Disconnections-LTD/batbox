#pragma once
#include <functional>
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace batbox::application_gadgets {
std::string findSimilarFile(const std::string& path);
std::optional<std::string> suggestPathUnderCwd(const std::string& requested,
  const std::string& cwd);

class IndigenousFileReadStash {
  struct Entry {
    std::string content,encoding;
    double mtimeMs=0;
    std::list<std::string>::iterator order;
  };
  mutable std::mutex mutex_;
  std::list<std::string> order_;
  std::unordered_map<std::string,Entry> stash_;
 public:
  struct ReadResult {std::string content,encoding;};
  struct Stats {std::size_t size;std::vector<std::string> entries;};
  ReadResult readFile(const std::string& path);
  std::string read(const std::string& path);
  void invalidate(const std::string& path);
  void clear();
  std::size_t size()const;
  Stats getStats()const;
};
} // namespace batbox::application_gadgets
