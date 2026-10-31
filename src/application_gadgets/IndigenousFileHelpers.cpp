#include "application_gadgets/IndigenousFileHelpers.hpp"
#include "filesystem/FileOps.hpp"
#include <filesystem>
#include <system_error>

namespace batbox::application_gadgets {
namespace fs=std::filesystem;
std::string findSimilarFile(const std::string& path){
  try{
    const fs::path requested(path);
    const auto base=requested.stem().string();
    for(const auto& file:fs::directory_iterator(requested.parent_path())){
      const auto name=file.path().filename();
      if(name.stem()==base&&(requested.parent_path()/name).string()!=path)
        return name.string();
    }
  }catch(const fs::filesystem_error&){}
  return {};
}

std::optional<std::string> suggestPathUnderCwd(const std::string& requested,
    const std::string& cwd){
  const fs::path base(cwd),parent=base.parent_path();
  fs::path resolved=requested;
  std::error_code error;
  const auto realParent=fs::canonical(resolved.parent_path(),error);
  if(!error)resolved=realParent/resolved.filename();
  const auto resolvedText=resolved.string();
  const auto cwdText=base.string();
  const auto parentText=parent.string();
  const auto parentPrefix=parentText=="/"?parentText:parentText+"/";
  if(!resolvedText.starts_with(parentPrefix)||resolvedText==cwdText||
      resolvedText.starts_with(cwdText+"/"))return std::nullopt;
  const auto relative=resolved.lexically_relative(parent);
  const auto corrected=base/relative;
  error.clear();
  if(fs::exists(corrected,error)&&!error)return corrected.string();
  return std::nullopt;
}

IndigenousFileReadStash::ReadResult IndigenousFileReadStash::readFile(const std::string& path){
  std::lock_guard lock(mutex_);
  double mtime;
  try{mtime=filesystem::getFileModificationTimeRaw(path);}
  catch(...){
    auto found=stash_.find(path);
    if(found!=stash_.end()){
      order_.erase(found->second.order);stash_.erase(found);
    }
    throw;
  }
  const auto found=stash_.find(path);
  if(found!=stash_.end()&&found->second.mtimeMs==mtime)
    return {found->second.content,found->second.encoding};
  auto file=filesystem::readFileSyncWithMetadata(path);
  if(found!=stash_.end()){
    found->second.content=file.content;
    found->second.encoding=file.encoding;
    found->second.mtimeMs=mtime;
  }else{
    order_.push_back(path);
    stash_.emplace(path,Entry{std::move(file.content),std::move(file.encoding),
      mtime,std::prev(order_.end())});
  }
  if(stash_.size()>1000){
    stash_.erase(order_.front());order_.pop_front();
  }
  const auto& stashed=stash_.at(path);
  return {stashed.content,stashed.encoding};
}
std::string IndigenousFileReadStash::read(const std::string& path){
  return readFile(path).content;
}
void IndigenousFileReadStash::invalidate(const std::string& path){
  std::lock_guard lock(mutex_);
  auto found=stash_.find(path);
  if(found==stash_.end())return;
  order_.erase(found->second.order);stash_.erase(found);
}
void IndigenousFileReadStash::clear(){
  std::lock_guard lock(mutex_);stash_.clear();order_.clear();
}
std::size_t IndigenousFileReadStash::size()const{
  std::lock_guard lock(mutex_);return stash_.size();
}
IndigenousFileReadStash::Stats IndigenousFileReadStash::getStats()const{
  std::lock_guard lock(mutex_);
  return {stash_.size(),{order_.begin(),order_.end()}};
}
} // namespace batbox::application_gadgets
