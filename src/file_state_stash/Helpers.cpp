#include "file_state_stash/FileStateStash.hpp"

namespace batbox::file_state_stash {
std::shared_ptr<FileStateStash> createFileStateStashWithSizeLimit(
  std::size_t maxEntries,std::size_t maxSizeBytes){
  return std::make_shared<FileStateStash>(maxEntries,maxSizeBytes);
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
} // namespace batbox::file_state_stash
