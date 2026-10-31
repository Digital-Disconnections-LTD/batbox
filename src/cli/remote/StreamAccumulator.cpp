#include "cli/remote/StreamAccumulator.hpp"
#include <stdexcept>

namespace batbox::cli::remote {
namespace {
std::string stringField(const Json& value,const char* name){
  const auto& part=value::property(value,name);
  if(!part.is_string())throw std::invalid_argument("Stream message field required");
  return part.get<std::string>();
}
std::string scopeKey(const Json& message){
  const auto& parent=value::property(message,"parent_tool_use_id");
  return stringField(message,"session_id")+":"+
    (parent.is_null()?std::string():parent.get<std::string>());
}
std::string blockKey(const Json& value){
  if(value.is_number())return value::numberToString(value.get<double>());
  if(value.is_string())return value.get<std::string>();
  throw std::invalid_argument("Stream block index required");
}
std::string joined(const std::vector<std::string>& chunks){
  std::string result;
  for(const auto& chunk:chunks)result+=chunk;
  return result;
}
}
StreamAccumulator createStreamAccumulator(){return {};}
std::vector<Json> accumulateStreamEvents(const std::vector<Json>& buffer,
    StreamAccumulator& state){
  std::vector<Json> output;
  std::map<const std::vector<std::string>*,std::size_t> touched;
  for(const auto& message:buffer){
    const auto& event=value::property(message,"event");
    const auto& kind=value::property(event,"type");
    if(kind=="message_start"){
      const auto id=stringField(value::property(event,"message"),"id");
      const auto scope=scopeKey(message);
      const auto previous=state.scopeToMessage.find(scope);
      if(previous!=state.scopeToMessage.end())
        state.byMessage.erase(previous->second);
      state.scopeToMessage[scope]=id;
      state.byMessage[id]={};
      output.push_back(message);continue;
    }
    if(kind=="content_block_delta"){
      const auto& delta=value::property(event,"delta");
      if(value::property(delta,"type")!="text_delta"){
        output.push_back(message);continue;
      }
      const auto scope=scopeKey(message);
      const auto active=state.scopeToMessage.find(scope);
      const auto blocks=active==state.scopeToMessage.end()?state.byMessage.end():
        state.byMessage.find(active->second);
      if(blocks==state.byMessage.end()){
        output.push_back(message);continue;
      }
      const auto key=blockKey(value::property(event,"index"));
      auto& chunks=blocks->second[key];
      if(!chunks)chunks=std::make_shared<std::vector<std::string>>();
      chunks->push_back(stringField(delta,"text"));
      const auto existing=touched.find(chunks.get());
      if(existing!=touched.end()){
        output[existing->second]["event"]["delta"]["text"]=joined(*chunks);
        continue;
      }
      Json snapshot={{"type","stream_event"},
        {"uuid",value::property(message,"uuid")},
        {"session_id",value::property(message,"session_id")},
        {"parent_tool_use_id",value::property(message,"parent_tool_use_id")},
        {"event",{{"type","content_block_delta"},
          {"index",value::property(event,"index")},
          {"delta",{{"type","text_delta"},{"text",joined(*chunks)}}}}}};
      touched[chunks.get()]=output.size();
      output.push_back(std::move(snapshot));continue;
    }
    output.push_back(message);
  }
  return output;
}
void clearStreamAccumulatorForMessage(StreamAccumulator& state,
    const Json& assistant){
  const auto id=stringField(value::property(assistant,"message"),"id");
  state.byMessage.erase(id);
  const auto scope=scopeKey(assistant);
  const auto found=state.scopeToMessage.find(scope);
  if(found!=state.scopeToMessage.end()&&found->second==id)
    state.scopeToMessage.erase(found);
}
}
