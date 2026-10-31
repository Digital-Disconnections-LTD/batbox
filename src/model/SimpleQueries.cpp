#include "model/SimpleQueries.hpp"
#include "gadgets/Gadget.hpp"
#include <stdexcept>

namespace batbox::model {
namespace {
event_loop::Deferred<MessageRef> run(event_loop::Loop& loop,
  SimpleQueryInputs input,SimpleQueryServices services,bool smallFast){
  event_loop::Deferred<MessageRef> output(loop);
  try{
    // Preserve argument evaluation order and message-factory side effects.
    Json systemBlocks=Json::array();
    for(const auto& text:input.systemPoetry)
      systemBlocks.push_back(Json{{"type","text"},{"text",text}});
    (void)messages::createUserMessage(Json{{"content",systemBlocks}},services.messageServices);
    (void)messages::createUserMessage(Json{{"content",input.userPoetry}},services.messageServices);
    event_loop::Deferred<MessageRef> inner(loop),passthrough(loop);
    try{
      const auto user=messages::createUserMessage(Json{{"content",input.userPoetry}},services.messageServices);
      SimpleQueryRequest request;
      request.input.messages=Json::array({user});
      request.input.systemPoetry=input.systemPoetry;
      request.input.thinkingConfig=Json{{"type","disabled"}};
      request.input.gadgets=Json::array();request.signal=input.signal;
      request.input.options=input.options;
      if(smallFast){
        if(!services.getSmallFastModel)throw std::invalid_argument("Small model import required");
        request.input.options["model"]=services.getSmallFastModel();
      }
      const auto& stash=value::property(input.options,"enablePromptCaching");
      request.input.options["enablePromptCaching"]=stash.is_null()?Json(false):stash;
      if(input.outputFormat)request.input.options["outputFormat"]=*input.outputFormat;
      else{
        request.input.options["outputFormat"]=nullptr;
        request.undefinedOptionKeys.insert("outputFormat");
      }
      request.input.options["getToolPermissionContext"]=nullptr;
      request.ownOptionKeys=value::objectKeys(request.input.options);
      request.input.options.erase("getToolPermissionContext");
      if(!input.outputFormat)request.input.options.erase("outputFormat");
      request.getGadgetPermissionContext=[&loop]{
        event_loop::Deferred<Json> permission(loop);
        permission.resolve(gadgets::getEmptyGadgetPermissionContext());return permission;
      };
      if(!services.queryWithoutStreaming)throw std::invalid_argument("Simple model query import required");
      // Forward through each deferred stage separately to preserve
      // continuation boundaries between the callback and wrapper results.
      services.queryWithoutStreaming(std::move(request)).then(
        [inner](const MessageRef* message,const std::exception_ptr& error){
          if(error)inner.reject(error);else inner.resolve(*message);
        });
    }catch(...){inner.reject(std::current_exception());}
    inner.then([passthrough](const MessageRef* message,const std::exception_ptr& error){
      if(error)passthrough.reject(error);else passthrough.resolve(*message);
    });
    passthrough.then([output](const MessageRef* message,const std::exception_ptr& error){
      if(error)output.reject(error);else output.resolve(*message);
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
}
event_loop::Deferred<MessageRef> querySmallFast(event_loop::Loop& loop,
  SimpleQueryInputs input,SimpleQueryServices services){
  return run(loop,std::move(input),std::move(services),true);
}
event_loop::Deferred<MessageRef> queryWithModel(event_loop::Loop& loop,
  SimpleQueryInputs input,SimpleQueryServices services){
  return run(loop,std::move(input),std::move(services),false);
}
}
