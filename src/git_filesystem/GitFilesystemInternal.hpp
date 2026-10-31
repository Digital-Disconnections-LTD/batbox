#pragma once
#include "git_filesystem/GitFilesystem.hpp"
#include <utility>

namespace batbox::git_filesystem {
struct GitFilesystem::Watcher {
  struct StashEntry {
    Json value;
    bool dirty=false;
    std::function<Deferred<Json>()> compute;
  };
  std::optional<std::string> gitDir,commonDir,branchRefPath;
  bool initialized=false;
  std::optional<Deferred<Unit>> initPromise;
  std::vector<std::string> watchedPaths;
  std::map<std::string,StashEntry> stash;
};
std::string joinPath(const std::string& first,const std::string& second);
std::string resolvePath(const std::string& first,const std::string& second);
std::string resolvePath(const std::string& path);
template<class T>Deferred<T> resolved(event_loop::Loop& loop,T value){
  Deferred<T> output(loop);output.resolve(std::move(value));return output;
}
template<class T>Deferred<T> rejected(event_loop::Loop& loop,std::exception_ptr error){
  Deferred<T> output(loop);output.reject(error);return output;
}
} // namespace batbox::git_filesystem
