#include "interactive/lifecycle/BatSubmit.hpp"
#include <stdexcept>

namespace batbox::interactive::lifecycle {
namespace {
template<class T>void need(const T& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("Bat-submit binding required: ")+name);
}
}
void onBatSubmit(const std::string& input,const BatSubmitMission& mission,
    const BatSubmitHelpers& helpers,
    std::shared_ptr<BatSubmitServices> services){
  need(services,"services");
  if(!services->owner.isOwnerThread())
    throw std::logic_error("Bat-submit owner required");
  if(mission.local){
    need(services->createUserMessage,"createUserMessage");
    need(services->appendMessageToLocalBat,"appendMessageToLocalAgent");
    services->appendMessageToLocalBat(mission.id,services->createUserMessage(input));
    if(mission.status=="running"){
      need(services->queuePendingMessage,"queuePendingMessage");
      services->queuePendingMessage(mission.id,input);
    }else{
      need(services->getMessages,"getMessages");
      need(services->getGadgetUseContext,"getToolUseContext");
      need(services->mainLoopModel,"mainLoopModel");
      need(services->canUseGadget,"canUseTool");
      need(services->resumeBatBackground,"resumeAgentBackground");
      auto abort=std::make_shared<TurnAbort>();
      auto context=services->getGadgetUseContext(services->getMessages(),{},
        abort,services->mainLoopModel());
      need(context,"Gadget use context");
      auto resume=services->resumeBatBackground(
        BatResumeRequest{mission.id,input,std::move(context),services->canUseGadget});
      if(!services->owner.dispatcher().sameLoop(resume.dispatcher()))
        throw std::invalid_argument("Bat resume Promise crossed owner loop");
      resume.then([services,id=mission.id](const event_loop::Unit*,
          const std::exception_ptr& error){
        if(!error)return;
        need(services->errorMessage,"errorMessage");
        const auto message=services->errorMessage(error);
        need(services->logForDebugging,"logForDebugging");
        services->logForDebugging("resumeAgentBackground failed: "+message);
        need(services->addLowPriorityNotification,"addLowPriorityNotification");
        services->addLowPriorityNotification("resume-agent-failed-"+id,
          "Failed to resume Bat: "+message);
      });
    }
  }else{
    need(services->injectUserMessageToPartner,"injectUserMessageToTeammate");
    services->injectUserMessageToPartner(mission.id,input);
  }
  need(services->setInputValue,"setInputValue");
  need(helpers.setCursorOffset,"setCursorOffset");
  need(helpers.clearBuffer,"clearBuffer");
  services->setInputValue("");
  helpers.setCursorOffset(0);
  helpers.clearBuffer();
}
} // namespace batbox::interactive::lifecycle
