#include "file_state_stash/FileStateStash.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::file_state_stash {
Json toJson(const FileState& value){
  Json result={{"content",value.content},{"timestamp",value.timestamp}};
  if(value.offset)result["offset"]=*value.offset;
  if(value.limit)result["limit"]=*value.limit;
  if(value.partialViewPresent||value.isPartialView)
    result["isPartialView"]=value.isPartialView;
  return result;
}
FileState fromJson(const Json& value){
  if(!value.is_object())throw std::invalid_argument("File state must be an object");
  FileState result;
  result.content=value.at("content").get<std::string>();
  result.timestamp=value.at("timestamp").get<double>();
  if(value.contains("offset")&&!value.at("offset").is_null())
    result.offset=value.at("offset").get<double>();
  if(value.contains("limit")&&!value.at("limit").is_null())
    result.limit=value.at("limit").get<double>();
  result.partialViewPresent=value.contains("isPartialView");
  if(result.partialViewPresent)result.isPartialView=value.at("isPartialView").get<bool>();
  return result;
}
FileStateStash::FileStateStash(std::size_t maxEntries,std::size_t maxSizeBytes):
  maxEntries_(maxEntries),maxSizeBytes_(maxSizeBytes){
  if(maxSizeBytes_==0)throw std::invalid_argument(
    "cannot set sizeCalculation without setting maxSize or maxEntrySize");
}
std::size_t FileStateStash::valueSize(const FileState& value){
  // WTF-8 lone surrogate bytes and Node's UTF-8 replacement both occupy
  // three bytes. Ordinary UTF-8 content has the same byte count.
  return std::max<std::size_t>(1,value.content.size());
}
FileStateStash::Value FileStateStash::get(const std::string& key){
  const auto found=index_.find(normalizeKey(key));
  if(found==index_.end())return {};
  order_.splice(order_.begin(),order_,found->second);
  return found->second->value;
}
bool FileStateStash::has(const std::string& key)const{
  return index_.contains(normalizeKey(key));
}
FileStateStash& FileStateStash::set(const std::string& key,FileState value){
  return set(key,std::make_shared<FileState>(std::move(value)));
}
FileStateStash& FileStateStash::set(const std::string& key,Value value){
  if(!value)return remove(key),*this;
  return setSized(normalizeKey(key),value,valueSize(*value));
}
FileStateStash& FileStateStash::setSized(std::string key,Value value,
  std::size_t newSize){
  const auto found=index_.find(key);
  if(newSize>maxSizeBytes_){
    if(found!=index_.end())erase(found->second);
    return *this;
  }
  if(found!=index_.end()){
    auto entry=found->second;
    order_.splice(order_.begin(),order_,entry);
    if(entry->value!=value){
      calculatedSize_-=entry->size;
      entry->size=newSize;
      calculatedSize_+=newSize;
      entry->value=std::move(value);
    }
  }else{
    order_.push_front({std::move(key),std::move(value),newSize});
    index_[order_.front().key]=order_.begin();
    calculatedSize_+=newSize;
  }
  while((maxEntries_&&index_.size()>maxEntries_)||calculatedSize_>maxSizeBytes_)
    evict();
  return *this;
}
void FileStateStash::erase(std::list<Entry>::iterator entry){
  calculatedSize_-=entry->size;
  index_.erase(entry->key);
  order_.erase(entry);
}
void FileStateStash::evict(){
  if(order_.empty())return;
  erase(std::prev(order_.end()));
}
bool FileStateStash::remove(const std::string& key){
  const auto found=index_.find(normalizeKey(key));
  if(found==index_.end())return false;
  erase(found->second);return true;
}
void FileStateStash::clear(){order_.clear();index_.clear();calculatedSize_=0;}
std::vector<std::string> FileStateStash::keys()const{
  std::vector<std::string> result;result.reserve(order_.size());
  for(const auto& entry:order_)result.push_back(entry.key);
  return result;
}
FileStateStash::Entries FileStateStash::entries()const{
  Entries result;result.reserve(order_.size());
  for(const auto& entry:order_)result.emplace_back(entry.key,entry.value);
  return result;
}
FileStateStash::Dump FileStateStash::dump()const{
  Dump result;result.reserve(order_.size());
  for(auto entry=order_.rbegin();entry!=order_.rend();++entry)
    result.push_back({entry->key,entry->value,entry->size});
  return result;
}
void FileStateStash::load(const Dump& entries){
  clear();
  for(const auto& entry:entries){
    if(entry.value)setSized(entry.key,entry.value,entry.size);
  }
}
Json FileStateStash::dumpJson()const{
  Json result=Json::array();
  for(const auto& entry:dump())result.push_back(Json::array({entry.key,
    Json{{"value",toJson(*entry.value)},{"size",entry.size}}}));
  return result;
}
void FileStateStash::loadJson(const Json& entries){
  if(!entries.is_array())throw std::invalid_argument("Cache dump must be an array");
  clear();
  for(const auto& item:entries){
    const auto& raw=item.at(1);
    auto value=std::make_shared<FileState>(fromJson(raw.at("value")));
    const auto& size=value::property(raw,"size");
    setSized(item.at(0).get<std::string>(),value,
      size.is_number_unsigned()?size.get<std::size_t>():
      size.is_number_integer()&&size.get<std::int64_t>()>0?
        static_cast<std::size_t>(size.get<std::int64_t>()):valueSize(*value));
  }
}
} // namespace batbox::file_state_stash
