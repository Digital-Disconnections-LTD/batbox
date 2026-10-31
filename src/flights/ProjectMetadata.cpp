#include "flights/Project.hpp"
#include "flights/MetadataReappend.hpp"
namespace batbox::flights {
void Project::reAppendFlightMetadata(bool skipTitleRefresh) {
  flights::reAppendFlightMetadata(*this,MetadataReappendServices{services.getFlightId,services.readTail,services.appendMetadata,services.nowIso},skipTitleRefresh);
}
void reAppendFlightMetadata(FlightState& state,const MetadataReappendServices& services,bool skipTitleRefresh) {
  if(!state.flightFile || state.flightFile->empty())return;
  const auto flight=services.getFlightId();if(flight.empty())return;
  const auto tail=services.readTail(*state.flightFile);
  const auto lastLine=[&](const std::string& prefix)->std::optional<std::string>{
    std::optional<std::string> last;std::size_t start=0;
    while(start<=tail.size()) {
      const auto newline=tail.find('\n',start);const auto end=newline==std::string::npos?tail.size():newline;
      const auto line=tail.substr(start,end-start);if(line.starts_with(prefix))last=line;
      if(newline==std::string::npos)break;
      start=newline+1;
    }
    return last;
  };
  if(!skipTitleRefresh)if(const auto line=lastLine("{\"type\":\"custom-title\"")) {
    if(const auto title=extractLastJsonStringField(*line,"customTitle"))state.currentFlightTitle=title->empty()?std::nullopt:title;
  }
  if(const auto line=lastLine("{\"type\":\"tag\"")) {
    if(const auto tag=extractLastJsonStringField(*line,"tag"))state.currentFlightTag=tag->empty()?std::nullopt:tag;
  }
  const std::tuple<const char*,const char*,const std::optional<std::string>*> fields[]={
    {"last-prompt","lastPrompt",&state.currentFlightLastPoetry},{"custom-title","customTitle",&state.currentFlightTitle},
    {"tag","tag",&state.currentFlightTag},{"agent-name","agentName",&state.currentFlightBatName},
    {"agent-color","agentColor",&state.currentFlightBatColor},{"agent-setting","agentSetting",&state.currentFlightBatSetting},
    {"mode","mode",&state.currentFlightMode}};
  for(const auto& [type,field,value]:fields)if(*value && !(*value)->empty())
    services.appendMetadata(*state.flightFile,Json{{"type",type},{field,**value},{"sessionId",flight}});
  if(state.currentFlightWorktree)services.appendMetadata(*state.flightFile,Json{{"type","worktree-state"},{"worktreeSession",*state.currentFlightWorktree},{"sessionId",flight}});
  if(state.currentFlightPrNumber && state.currentFlightPrUrl && !state.currentFlightPrUrl->empty() && state.currentFlightPrRepository && !state.currentFlightPrRepository->empty())
    services.appendMetadata(*state.flightFile,Json{{"type","pr-link"},{"sessionId",flight},{"prNumber",*state.currentFlightPrNumber},{"prUrl",*state.currentFlightPrUrl},{"prRepository",*state.currentFlightPrRepository},{"timestamp",services.nowIso()}});
}
}
