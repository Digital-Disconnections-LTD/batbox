#include "flights/Title.hpp"
#include "messages/TextContent.hpp"
#include <stdexcept>

namespace batbox::flights {
namespace {
// A title needs the gist of the first message, not a pasted file or log;
// the rest would only add to the request the server has to read.
constexpr std::size_t titleInputBytes=2000;
std::string titleInput(std::string text){
  if(text.size()<=titleInputBytes)return text;
  auto cut=titleInputBytes;
  while(cut>0&&(static_cast<unsigned char>(text[cut])&0xC0)==0x80)--cut;
  text.resize(cut);
  return text+"\n[...]";
}
}
event_loop::Deferred<std::optional<std::string>> generateFlightTitle(
  event_loop::Loop& loop,const std::string& description,std::stop_token signal,
  TitleServices services){
  event_loop::Deferred<std::optional<std::string>> result(loop);
  const auto trimmed=value::trim(description);
  if(trimmed.empty()){result.resolve(std::nullopt);return result;}
  const auto fail=[result,services](std::exception_ptr error){
    try{
      if(!services.debug||!services.errorToString)
        throw std::invalid_argument("Session title diagnostic imports required");
      services.debug("generateSessionTitle failed: "+services.errorToString(error),"error");
      result.resolve(std::nullopt);
    }catch(...){result.reject(std::current_exception());}
  };
  try{
    if(!services.querySmallFast||!services.isNonInteractiveFlight)
      throw std::invalid_argument("Session title query imports required");
    TitleQuery input;
    input.systemPoetry=Json::array({flightTitlePoetry()});
    input.userPoetry=titleInput(trimmed);input.signal=signal;
    input.outputFormat=Json{{"type","json_schema"},{"schema",{
      {"type","object"},{"properties",{{"title",{{"type","string"}}}}},
      {"required",Json::array({"title"})},{"additionalProperties",false}}}};
    input.options=Json{{"querySource","generate_session_title"},{"agents",Json::array()},
      {"isNonInteractiveSession",services.isNonInteractiveFlight()},
      {"hasAppendSystemPrompt",false},{"mcpTools",Json::array()}};
    services.querySmallFast(std::move(input)).then(
      [result,services,fail](const model::MessageRef* response,const std::exception_ptr& error){
        if(error){fail(error);return;}
        try{
          const auto text=messages::extractTextContent(value::property(
            value::property(**response,"message"),"content"));
          if(!services.safeParseJson)throw std::invalid_argument("Session title JSON parser required");
          const auto parsed=services.safeParseJson(text);
          const auto& title=value::property(parsed,"title");
          if(!parsed.is_object()||!title.is_string()){result.resolve(std::nullopt);return;}
          auto value=value::trim(title.get<std::string>());
          result.resolve(value.empty()?std::nullopt:std::optional<std::string>(std::move(value)));
        }catch(...){fail(std::current_exception());}
      });
  }catch(...){fail(std::current_exception());}
  return result;
}
}
