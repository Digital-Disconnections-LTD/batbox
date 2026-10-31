#include "interactive/cards_gadget_use/GadgetUseDecision.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::interactive::cards_gadget_use {
namespace {
void require(const auto& fn,const char* name){
  if(!fn)throw std::logic_error(std::string(name)+" import required");
}
std::string stringField(const Json& value,const char* name){
  const auto it=value.find(name);
  return it!=value.end()&&it->is_string()?it->get<std::string>():std::string{};
}
std::string caught(const std::exception& error){return error.what();}
}

GadgetUseCard classifyGadgetUse(const GadgetUseInputs& input,
                            const UseServices& services){
  require(services.readTerminalSize,"terminal size");
  const auto terminalSize=services.readTerminalSize();
  require(services.readTheme,"theme");
  const auto theme=services.readTheme();
  require(services.readSelectedBackground,"selected message background");
  const auto bg=services.readSelectedBackground();
  require(services.readPendingWorkerGadgetID,"pending worker request");
  const auto pending=services.readPendingWorkerGadgetID();
  require(services.readClassifierChecking,"classifier checking");
  const auto id=stringField(input.param,"id");
  (void)services.readClassifierChecking(id);
  require(services.readPermissionMode,"permission mode");
  const auto permissionMode=services.readPermissionMode();
  require(services.readStrippedDangerousRules,"stripped permission rules");
  const auto stripped=services.readStrippedDangerousRules();
  const bool isAutoClassifier=permissionMode=="auto"||
      (permissionMode=="plan"&&stripped);
  (void)isAutoClassifier; // Classifier display is disabled.

  const auto name=stringField(input.param,"name");
  const gadgets::Gadget* gadget=nullptr;
  if(input.gadgets)gadget=gadgets::findGadgetByName(*input.gadgets,name);
  if(!gadget){
    require(services.logError,"local diagnostic");
    services.logError(input.gadgets?"Gadget "+name+" not found":
                      "Gadgets array is undefined for Gadget "+name);
    return {};
  }
  require(services.safeParseInput,"Gadget input schema");
  const auto rawInput=input.param.value("input",Json());
  const auto parsed=services.safeParseInput(*gadget,rawInput);
  require(services.userFacingName,"Gadget display name");
  const auto userName=services.userFacingName(*gadget,parsed);
  Json nameBg=Json();
  if(gadget->userFacingNameBackgroundColor){
    require(services.userFacingNameBackgroundColor,"Gadget name background");
    nameBg=services.userFacingNameBackgroundColor(*gadget,parsed);
  }
  const bool transparent=gadget->isTransparentWrapper?gadget->isTransparentWrapper():false;
  if(!input.lookups||!input.inProgressGadgetUseIDs)
    throw std::logic_error("Gadget use lookups and in-progress IDs required");
  const bool resolved=input.lookups->resolvedGadgetUseIDs.contains(id);
  const bool queued=!input.inProgressGadgetUseIDs->contains(id)&&!resolved;
  const bool waiting=pending&&*pending==id;
  GadgetUseCard card;
  card.transparent=transparent;
  card.queued=queued;
  card.resolved=resolved;
  card.selectedBackground=bg;
  if(transparent){
    if(queued||resolved)return {};
    card.visible=true;
    card.progressKind=GadgetUseCard::ProgressKind::GadgetProgress;
  }else{
    if(userName.empty())return {};
    if(!parsed)return {};
    // The wrapper validates a second time, catches rendering errors,
    // and converts those errors to an empty use message.
    Json rendered="";
    try{
      const auto reparsed=services.safeParseInput(*gadget,*parsed);
      if(reparsed){
        if(!gadget->renderGadgetUseMessage)
          throw std::logic_error("Gadget.renderToolUseMessage is not a function");
        require(services.renderGadgetUse,"Gadget use renderer");
        rendered=services.renderGadgetUse(*gadget,*reparsed,
            UseRenderContext{theme,input.verbose,input.commands});
      }
    }catch(const std::exception& error){
      require(services.logError,"local diagnostic");
      services.logError("Error rendering Gadget use message for "+gadget->name+
                        ": "+caught(error));
    }
    if(rendered.is_null())return {};
    card.visible=true;
    card.name=userName;
    card.nameBackground=std::move(nameBg);
    card.useText=std::move(rendered);
    card.marginTop=input.addMargin?1:0;
    card.showDot=input.shouldShowDot;
    card.animate=input.shouldAnimate;
    card.error=input.lookups->erroredGadgetUseIDs.contains(id);
    require(services.stringWidth,"Gadget name width");
    card.nameWidth=services.stringWidth(userName)+(input.shouldShowDot?2:0);
    if(gadget->renderGadgetUseTag){
      require(services.renderTag,"Gadget use tag renderer");
      card.tag=services.renderTag(*gadget,*parsed);
    }
    if(!resolved&&!queued){
      card.progressKind=waiting?
          GadgetUseCard::ProgressKind::WaitingPermission:
          GadgetUseCard::ProgressKind::GadgetProgress;
    }
    if(!resolved&&queued){
      try{
        if(gadget->renderGadgetUseQueuedMessage){
          require(services.renderQueued,"Gadget queued renderer");
          card.queuedMessage=services.renderQueued(*gadget);
        }
      }catch(const std::exception& error){
        require(services.logError,"local diagnostic");
        services.logError("Error rendering Gadget use queued message for "+
                          gadget->name+": "+caught(error));
      }
    }
  }
  if(card.progressKind==GadgetUseCard::ProgressKind::GadgetProgress){
    try{
      if(gadget->renderGadgetUseProgressMessage){
        require(services.renderProgress,"Gadget progress renderer");
        card.progress=services.renderProgress(*gadget,
          gadgets::filterGadgetProgressMessages(input.progressMessages),
          ProgressRenderContext{input.gadgets,input.verbose,terminalSize,
            input.inProgressGadgetCallCount.value_or(1),input.transcriptMode});
      }
      card.preGadgetHook=cards_gadget_result::classifyHookProgress(
          "PreToolUse",id,input.transcriptMode,*input.lookups);
    }catch(const std::exception& error){
      require(services.logError,"local diagnostic");
      services.logError("Error rendering Gadget use progress message for "+
                        gadget->name+": "+caught(error));
      card.progress=Json();
      card.preGadgetHook={};
    }
  }
  return card;
}
}
