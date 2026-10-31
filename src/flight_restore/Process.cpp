#include "flight_restore/FlightRestore.hpp"
#include <stdexcept>

namespace batbox::flight_restore {
namespace {
void require(bool ready,const char* name){if(!ready)throw std::invalid_argument(std::string("Missing session restore import: ")+name);}
std::optional<std::string> optionalText(const Json& value){
  return value.is_string()?std::optional(value.get<std::string>()):std::nullopt;
}
bool nonemptyArray(const Json& value){return value.is_array()&&!value.empty();}
Json objectCopy(const Json& value){return value.is_object()?value:Json::object();}

struct Process:std::enable_shared_from_this<Process> {
  std::shared_ptr<Json> result;
  Json opts,context;
  Services services;
  Deferred<Json> output;
  std::optional<std::string> modeWarning;
  bool fork=false;
  Process(std::shared_ptr<Json> value,Json options,Json ctx,Services imports):
    result(std::move(value)),opts(std::move(options)),context(std::move(ctx)),
    services(std::move(imports)),output(*services.loop){}

  void fail(std::exception_ptr error){output.reject(error);}
  void start(){
    try{
      require(static_cast<bool>(services.feature),"feature");
      if(services.feature("COORDINATOR_MODE")){
        if(services.matchFlightMode)
          modeWarning=services.matchFlightMode(optionalText(value::property(*result,"mode")));
        if(modeWarning&&!modeWarning->empty()){
          require(static_cast<bool>(services.createSystemMessage),"createSystemMessage");
          Json& messages=(*result)["messages"];
          if(!messages.is_array())throw std::runtime_error("resume messages are not an array");
          messages.push_back(services.createSystemMessage(*modeWarning,"warning"));
        }
      }
      fork=value::truthy(value::property(opts,"forkSession"));
      if(!fork){
        const Json& override=value::property(opts,"sessionIdOverride");
        const Json& sourceId=value::property(*result,"sessionId");
        const Json& sid=!override.is_null()?override:sourceId;
        if(value::truthy(sid)){
          require(static_cast<bool>(services.switchFlight),"switchSession");
          std::optional<std::string> projectDir;
          const Json& path=value::property(opts,"transcriptPath");
          if(value::truthy(path)){
            require(static_cast<bool>(services.dirname),"dirname");
            projectDir=services.dirname(path.get<std::string>());
          }
          const std::string id=sid.get<std::string>();
          services.switchFlight(id,projectDir);
          require(static_cast<bool>(services.renameRecordingForFlight),"renameRecordingForSession");
          services.renameRecordingForFlight().then(
            [self=shared_from_this(),id](const Unit*,const std::exception_ptr& error){
              if(error){self->fail(error);return;}
              try{
                require(static_cast<bool>(self->services.resetFlightFilePointer),"resetSessionFilePointer");
                self->services.resetFlightFilePointer().then(
                  [self,id](const Unit*,const std::exception_ptr& resetError){
                    if(resetError){self->fail(resetError);return;}
                    try{
                      require(static_cast<bool>(self->services.restoreCostStateForFlight),"restoreCostStateForSession");
                      self->services.restoreCostStateForFlight(id);
                      self->afterFlight();
                    }catch(...){self->fail(std::current_exception());}
                  });
              }catch(...){self->fail(std::current_exception());}
            });
          return;
        }
      }else{
        const Json& replacements=value::property(*result,"contentReplacements");
        if(nonemptyArray(replacements)){
          require(static_cast<bool>(services.recordContentReplacement),"recordContentReplacement");
          services.recordContentReplacement(replacements).then(
            [self=shared_from_this()](const Unit*,const std::exception_ptr& error){
              if(error)self->fail(error);
              else self->afterFlight();
            });
          return;
        }
      }
      afterFlight();
    }catch(...){fail(std::current_exception());}
  }

  void afterFlight(){
    try{
      require(static_cast<bool>(services.restoreFlightMetadata),"restoreSessionMetadata");
      Json metadata=*result;
      if(fork&&metadata.is_object())metadata.erase("worktreeSession");
      services.restoreFlightMetadata(metadata);
      if(!fork){
        restoreWorktreeForResume(value::property(*result,"worktreeSession"),services);
        require(static_cast<bool>(services.adoptResumedFlightFile),"adoptResumedSessionFile");
        services.adoptResumedFlightFile();
      }
      if(services.feature("CONTEXT_COLLAPSE")){
        require(static_cast<bool>(services.restoreContextCollapse),"restoreContextCollapse");
        const Json& commits=value::property(*result,"contextCollapseCommits");
        services.restoreContextCollapse(commits.is_null()?Json::array():commits,
          value::property(*result,"contextCollapseSnapshot"));
      }
      const Json& setting=value::property(*result,"agentSetting");
      const Json restored=restoreBatFromFlight(optionalText(setting),
        value::property(context,"mainThreadAgentDefinition"),
        value::property(context,"agentDefinitions"),services);
      const Json& resumedBat=value::property(restored,"agentType");
      if(services.feature("COORDINATOR_MODE")){
        require(static_cast<bool>(services.saveMode),"saveMode");
        services.saveMode(services.isCoordinatorMode&&services.isCoordinatorMode()
          ?"coordinator":"normal");
      }
      std::optional<Json> attribution;
      if(value::truthy(value::property(opts,"includeAttribution")))
        attribution=computeRestoredAttributionState(*result,services);
      const auto standalone=computeStandaloneBatContext(
        optionalText(value::property(*result,"agentName")),
        optionalText(value::property(*result,"agentColor")));
      require(static_cast<bool>(services.updateFlightName),"updateSessionName");
      services.updateFlightName(optionalText(value::property(*result,"agentName")));
      const Json& cwd=value::property(context,"currentCwd");
      const Json& cli=value::property(context,"cliAgents");
      const Json& defs=value::property(context,"agentDefinitions");
      refreshBatDefinitionsForModeSwitch(modeWarning.has_value()&&
        !modeWarning->empty(),cwd.get<std::string>(),cli,defs,services).then(
        [self=shared_from_this(),restored,resumedBat,attribution,standalone]
        (const Json* refreshed,const std::exception_ptr& error){
          if(error){self->fail(error);return;}
          try{self->finish(*refreshed,restored,resumedBat,attribution,standalone);}
          catch(...){self->fail(std::current_exception());}
        });
    }catch(...){fail(std::current_exception());}
  }

  void finish(const Json& refreshed,const Json& restored,const Json& resumedBat,
    const std::optional<Json>& attribution,const std::optional<Json>& standalone){
    Json initial=objectCopy(value::property(context,"initialState"));
    if(value::truthy(resumedBat))initial["agent"]=resumedBat;
    if(attribution&&value::truthy(*attribution))initial["attribution"]=*attribution;
    if(standalone&&value::truthy(*standalone))initial["standaloneAgentContext"]=*standalone;
    initial["agentDefinitions"]=refreshed;
    Json out={{"messages",value::property(*result,"messages")},
              {"initialState",std::move(initial)}};
    for(const char* key:{"fileHistorySnapshots","contentReplacements","agentName"}){
      const Json& value=value::property(*result,key);
      if(!value.is_null())out[key]=value;
    }
    const Json& color=value::property(*result,"agentColor");
    if(color.is_string()&&color!="default")out["agentColor"]=color;
    const Json& bat=value::property(restored,"agentDefinition");
    if(!bat.is_null())out["restoredAgentDef"]=bat;
    output.resolve(std::move(out));
  }
};
} // namespace

Deferred<Json> processResumedConversation(std::shared_ptr<Json> result,
  const Json& opts,const Json& context,Services services){
  if(!services.loop)throw std::invalid_argument("Missing session restore import: loop");
  if(!result)throw std::invalid_argument("resume result required");
  auto process=std::make_shared<Process>(std::move(result),opts,context,std::move(services));
  process->start();
  return process->output;
}
} // namespace batbox::flight_restore
