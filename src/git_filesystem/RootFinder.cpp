#include "git_filesystem/RootFinder.hpp"
#include "git_filesystem/GitFilesystemInternal.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/Unicode.hpp"
#include "runtime/IndigenousPath.hpp"
#include <filesystem>
#include <stdexcept>
#include <system_error>

namespace batbox::git_filesystem {
RootFinder::RootFinder(std::function<FileStat(const std::string&)> statSync):
  statSync_(std::move(statSync)){
  if(!statSync_)throw std::invalid_argument("Git root statSync import required");
}
std::optional<std::string> RootFinder::compute(const std::string& startPath){
  auto current=resolvePath(startPath);
  constexpr char separator=static_cast<char>(std::filesystem::path::preferred_separator);
  const auto slash=current.find(separator);
  const auto root=slash==std::string::npos?std::string(1,separator):current.substr(0,slash+1);
  while(current!=root){
    try{
      const auto st=statSync_(joinPath(current,".git"));
      if(st.isDirectory||st.isFile)return value::normalizeNFC(current);
    }catch(...){}
    const auto parent=runtime::pathToUtf8(runtime::pathFromUtf8(current).parent_path());
    if(parent==current)break;
    current=parent;
  }
  try{
    const auto st=statSync_(joinPath(root,".git"));
    if(st.isDirectory||st.isFile)return value::normalizeNFC(root);
  }catch(...){}
  return std::nullopt;
}
std::optional<std::string> RootFinder::find(const std::string& startPath){
  const auto found=stash_.find(startPath);
  if(found!=stash_.end()){
    recency_.splice(recency_.begin(),recency_,found->second.recency);
    return found->second.value;
  }
  const auto value=compute(startPath);
  recency_.push_front(startPath);
  stash_.emplace(startPath,Entry{value,recency_.begin()});
  if(stash_.size()>50){stash_.erase(recency_.back());recency_.pop_back();}
  return value;
}
void RootFinder::clear(){stash_.clear();recency_.clear();}
std::size_t RootFinder::stashSize()const{return stash_.size();}
std::shared_ptr<RootFinder> indigenousRootFinder(){
  return std::make_shared<RootFinder>([](const std::string& path){
    std::error_code error;
    const auto status=std::filesystem::status(runtime::pathFromUtf8(value::encodeUtf8(path)),error);
    if(error)throw std::system_error(error,"git root statSync");
    return FileStat{std::filesystem::is_regular_file(status),
      std::filesystem::is_directory(status)};
  });
}
} // namespace batbox::git_filesystem
