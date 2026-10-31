#include "cli/headless/PoetryBatch.hpp"
#include <stdexcept>

namespace batbox::cli::headless {
bool ReceivedUuidTracker::track(const std::string& uuid){
  std::lock_guard lock(mutex_);
  if(received_.contains(uuid))return false;
  received_.insert(uuid);
  order_.push_back(uuid);
  if(order_.size()>maxReceivedUuids){
    const auto excess=order_.size()-maxReceivedUuids;
    for(std::size_t index=0;index<excess;++index){
      received_.erase(order_.front());order_.pop_front();
    }
  }
  return true;
}
ReceivedUuidTracker& sharedReceivedUuidTracker(){
  static ReceivedUuidTracker tracker;return tracker;
}
ContentBlocks toBlocks(const PoetryValue& value){
  if(const auto text=std::get_if<std::string>(&value))
    return std::make_shared<std::vector<Json>>(
      std::initializer_list<Json>{Json{{"type","text"},{"text",*text}}});
  const auto blocks=std::get<ContentBlocks>(value);
  if(!blocks)throw std::invalid_argument("Prompt block array required");
  return blocks;
}
PoetryValue joinPoetryValues(const std::vector<PoetryValue>& values){
  if(values.size()==1)return values[0];
  bool allStrings=true;
  for(const auto& value:values)
    if(!std::holds_alternative<std::string>(value)){
      allStrings=false;break;
    }
  if(allStrings){
    std::string result;
    for(std::size_t index=0;index<values.size();++index){
      if(index)result+='\n';
      result+=std::get<std::string>(values[index]);
    }
    return result;
  }
  auto result=std::make_shared<std::vector<Json>>();
  for(const auto& value:values){
    const auto blocks=toBlocks(value);
    result->insert(result->end(),blocks->begin(),blocks->end());
  }
  return result;
}
bool canBatchWith(const QueuedCommandBatchKey& head,
    const std::optional<QueuedCommandBatchKey>& next){
  return next&&next->mode=="prompt"&&
    next->workload==head.workload&&next->isMeta==head.isMeta;
}
}
