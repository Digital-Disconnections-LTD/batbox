#include "user_input/InputInternal.hpp"
#include "user_input/SlashInternal.hpp"
#include "user_input/CommandMessages.hpp"
#include <stdexcept>

namespace batbox::user_input::internal {
namespace {
void awaitRoute(const std::shared_ptr<InputState>& state,
  event_loop::Deferred<InputResult> pending){
  pending.then([state](const InputResult* value,const std::exception_ptr& error){
    if(error){fail(state,error);return;}
    finishBase(state,*value);
  });
}
void route(const std::shared_ptr<InputState>& state){
  try{
    const auto& request=state->request;
    if(request.mode=="bash"&&state->inputString){
      if(!state->services.processBashCommand)
        throw std::invalid_argument("Missing user input binding: processBashCommand");
      awaitRoute(state,state->services.processBashCommand(*state->inputString,
        state->preceding,state->attachments));
      return;
    }
    if(state->inputString&&!request.skipSlashCommands&&
       !state->inputString->empty()&&state->inputString->front()=='/'){
      awaitRoute(state,processSlashCommand(*state->inputString,
        state->preceding,state->imageBlocks,state->attachments,
        request.uuid,state->services.slash));
      return;
    }
    finishBase(state,processTextPoetry(state->normalizedInput,state->imageBlocks,
      state->imagePasteIds,state->attachments,request.uuid,
      state->permissionMode,request.isMeta,state->services.messages));
  }catch(...){fail(state,std::current_exception());}
}
void loadAttachments(const std::shared_ptr<InputState>& state,bool skipSlash){
  try{
    const auto& request=state->request;
    const bool shouldExtract=!request.skipAttachments&&state->inputString&&
      (request.mode!="prompt"||skipSlash||state->inputString->empty()||
       state->inputString->front()!='/');
    checkpoint(*state,"query_attachment_loading_start");
    if(!shouldExtract){
      checkpoint(*state,"query_attachment_loading_end");
      route(state);return;
    }
    if(!state->services.getAttachmentMessages)
      throw std::invalid_argument("Missing user input binding: getAttachmentMessages");
    auto pending=state->services.getAttachmentMessages(*state->inputString,
      request.ideSelection,Json::array(),request.messages,request.querySource);
    pending.then([state](const Json* value,const std::exception_ptr& error){
      if(error){fail(state,error);return;}
      try{
        if(!value->is_array())throw std::runtime_error("Attachment generator did not yield an array");
        state->attachments=*value;
        checkpoint(*state,"query_attachment_loading_end");
        route(state);
      }catch(...){fail(state,std::current_exception());}
    });
  }catch(...){fail(state,std::current_exception());}
}
}
void loadAttachmentsAndRoute(const std::shared_ptr<InputState>& state){
  try{
    const auto& request=state->request;
    bool skipSlash=request.skipSlashCommands;
    if(request.bridgeOrigin&&state->inputString&&
       !state->inputString->empty()&&state->inputString->front()=='/'){
      const auto parsed=commands::parseSlashCommand(*state->inputString);
      const auto* found=parsed?findEntry(parsed->commandName,
        state->services.slash.commands):nullptr;
      if(found){
        if(!state->services.isBridgeSafeCommand)
          throw std::invalid_argument("Missing user input binding: isBridgeSafeCommand");
        if(state->services.isBridgeSafeCommand(found->descriptor))skipSlash=false;
        else{
          const auto message="/"+nameOf(*found)+
            " isn't available over Remote Control.";
          InputResult result;
          Json args={{"content",*state->inputString}};
          if(request.uuid)args["uuid"]=*request.uuid;
          result.messages.push_back(messages::createUserMessage(args,state->services.messages));
          result.messages.push_back(createCommandInputMessage(
            "<local-command-stdout>"+message+"</local-command-stdout>",
            state->services.messages));
          result.resultText=message;
          finishBase(state,std::move(result));return;
        }
      }
    }
    if(!state->services.feature)
      throw std::invalid_argument("Missing user input binding: feature");
    if(state->services.feature("ULTRAPLAN")&&request.mode=="prompt"&&
       !request.isNonInteractiveFlight&&
       state->inputString&&!skipSlash&&
       (state->inputString->empty()||state->inputString->front()!='/')&&
       !value::truthy(value::property(state->services.getAppState(),"ultraplanSessionUrl"))&&
       !value::truthy(value::property(state->services.getAppState(),"ultraplanLaunching"))){
      if(!state->services.hasUltraplanKeyword||
         !state->services.replaceUltraplanKeyword)
        throw std::invalid_argument("Missing user input binding: ultraplan keyword");
      if(state->services.hasUltraplanKeyword(request.preExpansionInput.value_or(
           *state->inputString))){
        const auto rewritten=value::trim(state->services.replaceUltraplanKeyword(
          *state->inputString));
        awaitRoute(state,processSlashCommand("/ultraplan "+rewritten,
          state->preceding,state->imageBlocks,Json::array(),request.uuid,
          state->services.slash));
        return;
      }
    }
    // Keep this effective gate local; the original request remains unchanged.
    if(skipSlash!=request.skipSlashCommands)state->request.skipSlashCommands=skipSlash;
    loadAttachments(state,skipSlash);
  }catch(...){fail(state,std::current_exception());}
}
} // namespace batbox::user_input::internal
