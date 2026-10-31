#pragma once
#include "git_filesystem/GitFilesystem.hpp"
#include <functional>
#include <list>
#include <map>

namespace batbox::git_filesystem {
// Git-root lookup with an LRU of 50 entries.
class RootFinder {
public:
  explicit RootFinder(std::function<FileStat(const std::string&)> statSync);
  std::optional<std::string> find(const std::string& startPath);
  void clear();
  std::size_t stashSize()const;
private:
  struct Entry {std::optional<std::string> value;std::list<std::string>::iterator recency;};
  std::function<FileStat(const std::string&)> statSync_;
  std::list<std::string> recency_;
  std::map<std::string,Entry> stash_;
  std::optional<std::string> compute(const std::string& startPath);
};
std::shared_ptr<RootFinder> indigenousRootFinder();
} // namespace batbox::git_filesystem
