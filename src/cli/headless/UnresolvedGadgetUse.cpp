#include "cli/headless/UnresolvedGadgetUse.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::cli::headless {
namespace {
const Json& propertyOf(const Json& value,std::string_view key){
  if(value.is_null())throw std::runtime_error("Cannot read property of null");
  return value::property(value,key);
}
Json selectUnresolved(const flights::LoadedTranscript& transcript,
                      const std::string& gadgetUseId){
  Json gadgetUseMessage;
  for(const auto& message:transcript.messages.values()){
    const auto& type=propertyOf(message,"type");
    if(type=="assistant"){
      const auto& content=propertyOf(propertyOf(message,"message"),"content");
      if(content.is_array()){
        for(const auto& block:content){
          if(propertyOf(block,"type")=="tool_use"&&
             propertyOf(block,"id")==gadgetUseId){
            gadgetUseMessage=message;
            break;
          }
        }
      }
    }else if(type=="user"){
      const auto& content=propertyOf(propertyOf(message,"message"),"content");
      if(content.is_array()){
        for(const auto& block:content){
          if(propertyOf(block,"type")=="tool_result"&&
             propertyOf(block,"tool_use_id")==gadgetUseId)
            return nullptr;
        }
      }
    }
  }
  return gadgetUseMessage;
}
}
event_loop::Deferred<Json> findUnresolvedGadgetUse(
    event_loop::Loop& loop,const std::string& gadgetUseId,
    const UnresolvedGadgetUseServices& services){
  event_loop::Deferred<Json> result(loop);
  try{
    if(!services.getTranscriptPath||!services.loadTranscriptFile)
      throw std::invalid_argument("Transcript lookup imports required");
    const auto path=services.getTranscriptPath();
    services.loadTranscriptFile(path).then(
      [result,gadgetUseId](const flights::LoadedTranscript* transcript,
                         const std::exception_ptr& error){
        if(error){result.resolve(nullptr);return;}
        try{result.resolve(selectUnresolved(*transcript,gadgetUseId));}
        catch(...){result.resolve(nullptr);}
      });
  }catch(...){result.resolve(nullptr);}
  return result;
}
UnresolvedGadgetUseServices bindIndigenousUnresolvedGadgetUse(
    event_loop::Loop& loop,flights::Paths& paths,
    const flights::AsyncLoadServices& loadServices){
  return {
    [&paths]{return paths.getTranscriptPath();},
    [&loop,loadServices](const std::string& path){
      return flights::loadTranscriptFileAsync(loop,path,false,loadServices);
    }
  };
}
}
