#include "application_skills/Provider.hpp"
#include "application_skills/ShellExpansion.hpp"
#include "application/Async.hpp"
#include "bats/ForkedBat.hpp"
#include "gadgets/live_roster/LiveRoster.hpp"
#include <stdexcept>

namespace batbox::application_skills {
namespace {
std::string field(const Json& value,const char* key){
  const auto& item=value::property(value,key);
  return item.is_string()?item.get<std::string>():std::string{};
}
}
std::string Provider::executePoetryShell(const std::string& text,
    const std::string& slashCommandName,const std::optional<std::string>& shell,
    const std::vector<std::string>& allowedGadgets,
    const bats::BatContext& context){
  if(parsePoetryShellMatches(text).empty())return text;
  if(inputs_.owner.isOwnerThread())throw std::logic_error(
    "Skill shell expansion requires a worker while owner callbacks are active");
  // Skill shell expansion currently uses Bash, including requests for
  // PowerShell; there is no separate PowerShell gadget here.
  (void)shell;
  const auto dispatcher=inputs_.owner.dispatcher();
  std::shared_ptr<const gadgets::live_roster::Snapshot> current;
  const gadgets::Gadget* bash=gadgets::findGadgetByName(context.query.gadgets,"Bash");
  if(!bash&&inputs_.currentGadgetSnapshot){
    current=application::onOwnerAndWait<
      std::shared_ptr<const gadgets::live_roster::Snapshot>>(
        dispatcher,inputs_.currentGadgetSnapshot);
    if(current)bash=current->find("Bash");
  }
  if(!bash||!bash->call||!bash->checkPermissions||
      !bash->mapGadgetResultToGadgetResultBlockParam)
    throw skills::process::MalformedCommandError(
      "[Error]\nBash tool is unavailable for skill shell expansion");
  auto baseState=bats::effectiveBatGetAppState(context);
  if(!baseState)baseState=inputs_.appState;
  const auto getState=bats::createGetAppStateWithAllowedGadgets(
    [dispatcher,baseState]{
      return application::onOwnerAndWait<Json>(dispatcher,baseState);
    },allowedGadgets);
  const auto storage=inputs_.resultStorage;
  auto* loop=&inputs_.owner;
  const auto uuid=inputs_.messages->randomUUID;
  const auto diagnostic=inputs_.diagnostic;
  return expandPoetryShell(text,slashCommandName,
    [bash,context,getState,storage,dispatcher,loop,uuid,
        slashCommandName,diagnostic](const std::string& command,
        const std::string& pattern){
      const Json input={{"command",command}};
      const Json state=getState();
      if(context.query.stop.stop_requested()||
          (context.abortOwner&&context.abortOwner->stop_requested()))
        throw std::runtime_error("Request aborted");
      const Json decision=bash->checkPermissions(input,state).get();
      if(field(decision,"behavior")!="allow"){
        const auto reason=field(decision,"message");
        if(diagnostic)diagnostic("Shell command permission check failed for command in "+
          slashCommandName+": "+command+". Error: "+reason);
        throw skills::process::MalformedCommandError(
          "Shell command permission check failed for pattern \""+pattern+
          "\": "+(reason.empty()?"Permission denied":reason));
      }
      gadgets::GadgetCallContext call;
      call.data=context.query.data.is_object()?context.query.data:Json::object();
      call.data["toolPermissionContext"]=
        value::property(state,"toolPermissionContext");
      call.stopToken=context.query.stop.stop_possible()?context.query.stop:
        context.abortOwner?context.abortOwner->get_token():std::stop_token{};
      call.currentAbortReason=context.currentAbortReason;
      call.batContext=std::make_shared<bats::BatContext>(context);
      call.readFileState=context.query.readFileState;
      call.nestedMemoryAttachmentTriggers=context.query.nestedMemoryAttachmentTriggers;
      call.loadedNestedMemoryPaths=context.query.loadedNestedMemoryPaths;
      call.dynamicSkillDirTriggers=context.query.dynamicSkillDirTriggers;
      call.discoveredSkillNames=context.query.discoveredSkillNames;
      const auto data=bash->call(input,call,{},Json(),{}).get().data;
      const Json block=application::awaitOnOwner<Json>(dispatcher,
        [loop,storage,bash,data,id=uuid()]{
          return gadget_results::processGadgetResultBlock(*loop,storage,bash->name,
            bash->maxResultSizeChars,
            bash->mapGadgetResultToGadgetResultBlockParam,data,id);
        });
      const auto& content=value::property(block,"content");
      return content.is_string()?content.get<std::string>():
        formatPoetryShellOutput(field(data,"stdout"),field(data,"stderr"));
    });
}
}
