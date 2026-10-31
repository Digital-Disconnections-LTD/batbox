#include "memory_prefetch/IndigenousIO.hpp"
#include <chrono>

namespace batbox::memory_prefetch {
namespace {
double nowMillis(const Services& services){
  if(services.nowMillis)return services.nowMillis();
  return std::chrono::duration<double,std::milli>(
    std::chrono::system_clock::now().time_since_epoch()).count();
}
struct ReadState {
  event_loop::Deferred<Json> output;
  std::vector<std::optional<Json>> items;
  std::size_t remaining=0;
  explicit ReadState(event_loop::Loop& loop,std::size_t size):
    output(loop),items(size),remaining(size){}
};
}
std::string memoryHeader(const std::string& path,double mtimeMs,double nowMs){
  const auto freshness=instructions::memoryFreshnessText(mtimeMs,nowMs);
  return freshness.empty()?"Memory (saved "+instructions::memoryAge(mtimeMs,nowMs)+"): "+path+":"
    :freshness+"\n\nMemory: "+path+":";
}
event_loop::Deferred<Json> readMemoriesForSurfacing(
  const std::vector<RelevantMemory>& selected,std::stop_token signal,
  const Services& services){
  auto state=std::make_shared<ReadState>(services.loop,selected.size());
  if(selected.empty()){state->output.resolve(Json::array());return state->output;}
  auto finish=[state]{
    if(--state->remaining!=0)return;
    Json results=Json::array();
    for(const auto& item:state->items)if(item)results.push_back(*item);
    state->output.resolve(std::move(results));
  };
  for(std::size_t index=0;index<selected.size();++index){
    const auto memory=selected[index];
    try{
      auto pending=readMemoryRangeAsync(memory.path,signal,services.loop);
      pending.then([state,index,memory,services,finish](
      const filesystem::ReadFileRangeResult* value,
      const std::exception_ptr& error){
      if(!error&&value){
        try{
          const bool truncated=value->totalLines>200||value->truncatedByBytes;
          std::string content=value->content;
          if(truncated)content+="\n\n> This memory file was truncated ("+
            std::string(value->truncatedByBytes?"4096 byte limit":"first 200 lines")+
            "). Use the Read tool to view the complete file at: "+memory.path;
          Json item={{"path",memory.path},{"content",content},
            {"mtimeMs",memory.mtimeMs},
            {"header",memoryHeader(memory.path,memory.mtimeMs,nowMillis(services))}};
          if(truncated)item["limit"]=value->lineCount;
          state->items[index]=std::move(item);
        }catch(const std::exception&){}
      }
      finish();
      });
    }catch(const std::exception&){finish();}
  }
  return state->output;
}
} // namespace batbox::memory_prefetch
