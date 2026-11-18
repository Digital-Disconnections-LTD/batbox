#pragma once
#include "git_filesystem/GitFilesystem.hpp"
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>

namespace batbox::git {
struct CanonicalRootImports {
  std::function<git_filesystem::FileStat(const std::string&)> statSync;
  std::function<std::string(const std::string&)> readFileSync;
  std::function<std::string(const std::string&)> realpathSync;
};

// Git-root discovery (upward walk for .git) and worktree canonicalization in
// one finder behind a single LRU of 50 entries. This was two finders --
// git_filesystem::RootFinder (walk + cache) and CanonicalRoot (canonicalize
// + a second cache of the same shape) -- composed through a findGitRoot
// function. One system finding git roots, one cache holding per key two
// facts: the nearest root and the canonical root.
class CanonicalRoot {
public:
  explicit CanonicalRoot(CanonicalRootImports imports);
  // Nearest git root at or above startPath, canonicalized.
  std::optional<std::string> find(const std::string& startPath);
  // Nearest git root at or above startPath, not canonicalized: a worktree's
  // own directory, whose .git file points at its private gitdir.
  std::optional<std::string> nearest(const std::string& startPath);
  // Canonical root for a known git root (.git file worktree resolution).
  std::string resolve(const std::string& gitRoot);
private:
  struct Entry {
    std::optional<std::string> root;   // nearest walk result
    std::optional<std::string> value;  // canonical root
    std::list<std::string>::iterator recency;
  };
  CanonicalRootImports imports_;
  std::list<std::string> recency_;
  std::map<std::string,Entry> stash_;
  std::optional<std::string> walk(const std::string& startPath) const;
  std::string canonical(const std::string& gitRoot) const;
  Entry& touch(const std::string& key);
};
CanonicalRootImports indigenousCanonicalRootImports();
std::shared_ptr<CanonicalRoot> indigenousCanonicalRoot();
} // namespace batbox::git
