#include "memory_prefetch/Prefetch.hpp"
#include "memory_prefetch/PoetryScan.hpp"
#include "messages/TextContent.hpp"
#include <atomic>
#include <stdexcept>

namespace batbox::memory_prefetch {
namespace {
struct State {
  std::stop_source stop;
  std::shared_ptr<std::stop_callback<std::function<void()>>> parentLink;
  std::atomic_bool settled=false;
  Json result=Json::array();
  std::exception_ptr error;
};
bool jsWhitespace(std::uint16_t unit){
  return (unit>=9&&unit<=13)||unit==32||unit==0xA0||unit==0x1680||
    (unit>=0x2000&&unit<=0x200A)||unit==0x2028||unit==0x2029||
    unit==0x202F||unit==0x205F||unit==0x3000||unit==0xFEFF;
}
bool hasWhitespace(const std::string& text){
  for(const auto unit:value::utf16CodeUnits(text))if(jsWhitespace(unit))return true;
  return false;
}
bool aborted(const std::exception_ptr& error,const Services& services,
  std::stop_token token){
  if(token.stop_requested())return true;
  if(!services.isAbortError)
    throw std::invalid_argument("Memory abort classifier required");
  try{std::rethrow_exception(error);}
  catch(const std::exception& failure){return services.isAbortError(failure);}
  catch(...){return false;}
}
void settleFailure(const std::shared_ptr<State>& state,const Services& services,
  std::exception_ptr error){
  try{
    if(!aborted(error,services,state->stop.get_token())){
      if(!services.logError)
        throw std::invalid_argument("Memory prefetch error logger required");
      services.logError(error);
    }
  }catch(...){state->error=std::current_exception();}
  state->result=Json::array();
  state->settled.store(true,std::memory_order_release);
  if(services.prefetchFinished)services.prefetchFinished(state->stop);
}
}
query::MemoryPrefetch startRelevantMemoryPrefetch(const Json& messages,
  const execution::Context& gadgetUseContext,const Services& services){
  if(!services.loop.isOwnerThread())
    throw std::logic_error("Memory prefetch requires owner event loop");
  // Maintenance already has a memory manifest and the parent's stash-safe
  // context. Do not recursively start another selector for its own poetry.
  if(value::truthy(value::property(gadgetUseContext.data,"memoryMaintenance")))return {};
  if(!instructions::isAutoMemoryEnabled(services.paths))return {};
  if(!services.searchFeatureEnabled)
    throw std::invalid_argument("Memory prefetch feature binding required");
  if(!services.searchFeatureEnabled())return {};
  if(!messages.is_array())throw std::invalid_argument("Messages must be an array");
  std::optional<std::size_t> last;
  for(std::size_t i=messages.size();i>0;){
    --i;const Json& message=messages[i];
    if(value::property(message,"type")=="user"&&
       !value::truthy(value::property(message,"isMeta"))){last=i;break;}
  }
  if(!last)return {};
  const auto input=messages::getContentText(
    value::property(value::property(messages[*last],"message"),"content"));
  if(!input||!hasWhitespace(value::trim(*input)))return {};
  const auto surfaced=collectSurfacedMemories(messages);
  if(surfaced.totalBytes>=60*1024)return {};
  if(!gadgetUseContext.readFileState)
    throw std::invalid_argument("Memory prefetch requires typed file cache");
  auto state=std::make_shared<State>();
  state->parentLink=std::make_shared<std::stop_callback<std::function<void()>>>(
    gadgetUseContext.stop,[source=state->stop]() mutable {source.request_stop();});
  std::unordered_set<std::string> prior(surfaced.paths.begin(),surfaced.paths.end());
  const Json& bats=value::property(value::property(value::property(
    gadgetUseContext.data,"options"),"agentDefinitions"),"activeAgents");
  if(services.prefetchStarted)services.prefetchStarted(state->stop);
  try{
    auto pending=getRelevantMemoryAttachments(*input,bats,
      gadgetUseContext.readFileState,
      collectRecentSuccessfulGadgets(messages,*last),state->stop.get_token(),
      prior,services);
    pending.then([state,services](const Json* value,const std::exception_ptr& error){
      if(error){settleFailure(state,services,error);return;}
      state->result=value?*value:Json::array();
      state->settled.store(true,std::memory_order_release);
      if(services.prefetchFinished)services.prefetchFinished(state->stop);
    });
  }catch(...){settleFailure(state,services,std::current_exception());}
  query::MemoryPrefetch handle;
  handle.isSettled=[state]{return state->settled.load(std::memory_order_acquire);};
  handle.read=[state]{
    if(!state->settled.load(std::memory_order_acquire))
      throw std::logic_error("Memory prefetch is pending");
    if(state->error)std::rethrow_exception(state->error);
    return state->result;
  };
  handle.dispose=[state]{state->stop.request_stop();};
  return handle;
}
} // namespace batbox::memory_prefetch
