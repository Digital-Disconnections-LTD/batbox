#pragma once
#include <functional>
#include <list>
#include <map>
#include <optional>
#include <string>

namespace batbox::git {
struct CanonicalRootImports {
  std::function<std::optional<std::string>(const std::string&)> findGitRoot;
  std::function<std::string(const std::string&)> readFileSync;
  std::function<std::string(const std::string&)> realpathSync;
};

// Canonical-root resolution with an LRU of 50 entries and a Git-root wrapper.
class CanonicalRoot {
public:
  explicit CanonicalRoot(CanonicalRootImports imports);
  std::optional<std::string> find(const std::string& startPath);
  std::string resolve(const std::string& gitRoot);
  void clear();
  std::size_t stashSize()const;
  bool stashDelete(const std::string& root);
  std::optional<std::string> stashGet(const std::string& root)const;
  bool stashHas(const std::string& root)const;
private:
  struct Entry {std::string value;std::list<std::string>::iterator recency;};
  CanonicalRootImports imports_;
  std::list<std::string> recency_;
  std::map<std::string,Entry> stash_;
  std::string compute(const std::string& gitRoot)const;
};
CanonicalRootImports indigenousCanonicalRootImports();
} // namespace batbox::git
