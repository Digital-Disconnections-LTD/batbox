#include "file_state_stash/FileStateStash.hpp"

namespace batbox::file_state_stash {
std::shared_ptr<FileStateStash> createFileStateStashWithSizeLimit(
  std::size_t maxEntries,std::size_t maxSizeBytes){
  return std::make_shared<FileStateStash>(maxEntries,maxSizeBytes);
}
FileStateStash::Entries stashToObject(const FileStateStash& stash){
  return stash.entries();
}
Json stashToJson(const FileStateStash& stash){
  Json object=Json::object();
  for(const auto& [key,value]:stash.entries())object[key]=toJson(*value);
  return object;
}
std::vector<std::string> stashKeys(const FileStateStash& stash){return stash.keys();}
std::shared_ptr<FileStateStash> cloneFileStateStash(const FileStateStash& stash){
  auto clone=createFileStateStashWithSizeLimit(stash.max(),stash.maxSize());
  clone->load(stash.dump());
  return clone;
}
std::shared_ptr<FileStateStash> mergeFileStateStashes(
  const FileStateStash& first,const FileStateStash& second){
  auto merged=cloneFileStateStash(first);
  for(const auto& [path,state]:second.entries()){
    const auto existing=merged->get(path);
    if(!existing||state->timestamp>existing->timestamp)merged->set(path,state);
  }
  return merged;
}
} // namespace batbox::file_state_stash
