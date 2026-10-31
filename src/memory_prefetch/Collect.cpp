#include "memory_prefetch/BatMemoryPath.hpp"
#include "memory_prefetch/Prefetch.hpp"
#include "memory_prefetch/PoetryScan.hpp"
#include <stdexcept>

namespace batbox::memory_prefetch {
namespace {
struct SearchState {
  event_loop::Deferred<Json> output;
  std::vector<std::vector<RelevantMemory>> results;
  std::size_t remaining=0;
  SearchState(event_loop::Loop& loop,std::size_t size):
    output(loop),results(size),remaining(size){}
};
std::vector<RelevantMemory> selectedMemories(
    const std::vector<std::vector<RelevantMemory>>& groups,
    file_state_stash::FileStateStash& stash,
    const std::unordered_set<std::string>& surfaced){
  std::vector<RelevantMemory> selected;
  for(const auto& group:groups)for(const auto& memory:group){
    if(stash.has(memory.path)||surfaced.contains(memory.path))continue;
    selected.push_back(memory);
    if(selected.size()==5)return selected;
  }
  return selected;
}
}
event_loop::Deferred<Json> getRelevantMemoryAttachments(
  const std::string& input,const Json& bats,
  const std::shared_ptr<file_state_stash::FileStateStash>& readFileState,
  const std::vector<std::string>& recentGadgets,std::stop_token signal,
  const std::unordered_set<std::string>& alreadySurfaced,
  const Services& services){
  if(!readFileState)throw std::invalid_argument("Memory search requires file cache");
  std::vector<std::string> dirs;
  for(auto mention:extractBatMentions(input)){
    const auto prefix=mention.find("agent-");
    if(prefix!=std::string::npos)mention.erase(prefix,6);
    if(!bats.is_array())continue;
    for(const Json& bat:bats){
      if(value::property(bat,"agentType")!=mention)continue;
      const Json& scope=value::property(bat,"memory");
      if(value::truthy(scope)&&scope.is_string())
        dirs.push_back(getBatMemoryDir(mention,scope.get<std::string>(),
          services.cwd,services.paths));
      break;
    }
  }
  if(dirs.empty()){
    if(!services.pathStash)throw std::invalid_argument("Memory path cache required");
    dirs.push_back(instructions::getAutoMemPath(services.paths,*services.pathStash));
  }
  auto state=std::make_shared<SearchState>(services.loop,dirs.size());
  auto finish=[state,readFileState,alreadySurfaced,signal,services]{
    try{
      auto selected=selectedMemories(state->results,*readFileState,
        alreadySurfaced);
      auto read=readMemoriesForSurfacing(selected,signal,services);
      read.then([state](const Json* memories,const std::exception_ptr& error){
        if(error){state->output.reject(error);return;}
        if(!memories||memories->empty()){state->output.resolve(Json::array());return;}
        state->output.resolve(Json::array({Json{{"type","relevant_memories"},
          {"memories",*memories}}}));
      });
    }catch(...){state->output.reject(std::current_exception());}
  };
  for(std::size_t index=0;index<dirs.size();++index){
    try{
      auto pending=findRelevantMemories(input,dirs[index],recentGadgets,
        alreadySurfaced,signal,services);
      pending.then([state,index,finish](
          const std::vector<RelevantMemory>* value,
          const std::exception_ptr&){
        if(value)state->results[index]=*value;
        if(--state->remaining==0)finish();
      });
    }catch(...){
      // A rejected directory search contributes [] to Promise.all.
      if(--state->remaining==0)finish();
    }
  }
  return state->output;
}
} // namespace batbox::memory_prefetch
