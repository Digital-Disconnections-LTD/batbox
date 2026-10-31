#include "user_input/SlashInternal.hpp"
#include "user_input/CommandMessages.hpp"

namespace batbox::user_input::internal {
event_loop::Deferred<InputResult> runUiCommand(const CommandEntry& entry,
  const std::string& args,const Json& preceding,SlashServices& services){
  event_loop::Deferred<InputResult> output(services.loop);
  if(!entry.invokeUi){
    output.reject(std::make_exception_ptr(std::invalid_argument(
      "Missing command binding: invokeUi")));
    return output;
  }
  try{
  auto pending=entry.invokeUi(args);
  const auto displayName=nameOf(entry);
  pending.then([output,displayName,args,preceding,&services]
    (const UiCompletion* completed,const std::exception_ptr& error){
    try{
      // CommandUi converts rejected loading/invocation to an empty completion.
      // A rejected integration binding also clears the command here.
      if(error){
        if(services.logError)services.logError(errorText(error));
        output.resolve(InputResult{});
        return;
      }
      const auto& value=*completed;
      InputResult result;
      result.nextInput=value.nextInput;
      result.submitNextInput=value.submitNextInput;
      if(value.display=="skip"){
        output.resolve(std::move(result));
        return;
      }
      Json meta=Json::array();
      for(const auto& content:value.metaMessages)
        meta.push_back(messages::createUserMessage(
          Json{{"content",content},{"isMeta",true}},services.messages));
      const auto formatted=formatCommandInputTags(displayName,args);
      bool skipTranscript=false;
      if(services.isFullscreen&&services.isFullscreen()&&value.result){
        constexpr std::string_view suffix=" dismissed";
        const auto& text=*value.result;
        skipTranscript=text.size()>=suffix.size()&&
          text.compare(text.size()-suffix.size(),suffix.size(),suffix)==0;
      }
      if(value.display=="system"&&!skipTranscript){
        result.messages.push_back(createCommandInputMessage(formatted,services.messages));
        result.messages.push_back(createCommandInputMessage(
          "<local-command-stdout>"+value.result.value_or("undefined")+
          "</local-command-stdout>",services.messages));
      }else if(value.display!="system"){
        result.messages.push_back(userMessage(formatted,preceding,services.messages));
        const auto display=value.result&& !value.result->empty()
          ?*value.result:"(no content)";
        result.messages.push_back(messages::createUserMessage(Json{{"content",
          "<local-command-stdout>"+display+"</local-command-stdout>"}},
          services.messages));
      }
      for(const auto& message:meta)result.messages.push_back(message);
      result.shouldQuery=value.shouldQuery;
      output.resolve(std::move(result));
    }catch(...){output.reject(std::current_exception());}
  });
  }catch(...){
    if(services.logError)services.logError(errorText(std::current_exception()));
    output.resolve(InputResult{});
  }
  return output;
}
} // namespace batbox::user_input::internal
