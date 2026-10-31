#include "user_input/SlashDispatch.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using namespace batbox;
namespace {
Json project(const Json& message){
  Json out={{"type",value::property(message,"type")}};
  if(value::property(message,"type")=="user")
    out["content"]=message.at("message").at("content");
  else if(message.contains("content"))out["content"]=message.at("content");
  for(const auto* key:{"subtype","isMeta","level"})
    if(message.contains(key))out[key]=message.at(key);
  return out;
}
Json output(const user_input::InputResult& value){
  Json messages=Json::array();
  for(const auto& message:value.messages)messages.push_back(project(message));
  Json out={{"messages",messages},{"shouldQuery",value.shouldQuery}};
  if(value.allowedGadgets)out["allowedTools"]=*value.allowedGadgets;
  if(value.model)out["model"]=*value.model;
  if(value.effort)out["effort"]=*value.effort;
  if(value.resultText)out["resultText"]=*value.resultText;
  if(value.nextInput)out["nextInput"]=*value.nextInput;
  if(value.submitNextInput)out["submitNextInput"]=*value.submitNextInput;
  return out;
}
}
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    event_loop::Loop loop;
    messages::Services messageServices;
    messageServices.randomUUID=[]{return "uuid";};
    messageServices.nowISO=[]{return "now";};
    const Json test=value::parse(line);
    std::vector<user_input::CommandEntry> entries;
    const auto& raw=value::property(test,"command");
    if(raw.is_object()){
      user_input::CommandEntry entry;
      entry.descriptor=raw;
      if(!entry.descriptor.contains("name"))entry.descriptor["name"]="help";
      if(!entry.descriptor.contains("type"))entry.descriptor["type"]="local";
      entry.loadLocal=[&loop,test]{
        event_loop::Deferred<user_input::LocalCall> loaded(loop);
        loaded.resolve([&loop,test](const std::string&){
          event_loop::Deferred<Json> result(loop);
          if(value::property(test,"callError")==true)
            result.reject(std::make_exception_ptr(std::runtime_error("local failed")));
          else{
            const auto& value=value::property(test,"localResult");
            result.resolve(value.is_null()?Json{{"type","text"},{"value","done"}}:value);
          }
          return result;
        });
        return loaded;
      };
      entry.invokeUi=[&loop,test](const std::string&){
        event_loop::Deferred<user_input::UiCompletion> result(loop);
        if(value::property(test,"callError")==true){
          result.reject(std::make_exception_ptr(std::runtime_error("local failed")));
          return result;
        }
        user_input::UiCompletion value;
        const auto& rawResult=value::property(test,"uiResult");
        if(rawResult.is_string())value.result=rawResult.get<std::string>();
        const auto& options=value::property(test,"uiOptions");
        if(value::property(options,"display").is_string())
          value.display=value::property(options,"display").get<std::string>();
        value.shouldQuery=value::property(options,"shouldQuery")==true;
        const auto& meta=value::property(options,"metaMessages");
        if(meta.is_array())for(const auto& item:meta)value.metaMessages.push_back(item.get<std::string>());
        if(value::property(options,"nextInput").is_string())
          value.nextInput=value::property(options,"nextInput").get<std::string>();
        if(options.is_object()&&options.contains("submitNextInput"))
          value.submitNextInput=options.at("submitNextInput").get<bool>();
        result.resolve(std::move(value));return result;
      };
      entry.expandPoetry=[&loop](const std::string& args,const Json&,const Json&,
        const std::optional<std::string>&){
        event_loop::Deferred<user_input::InputResult> result(loop);
        user_input::InputResult value;
        value.messages.push_back(Json{{"type","user"},{"message",Json{{"content","prompt:"+args}}}});
        value.shouldQuery=true;value.allowedGadgets=Json::array({"Read"});
        value.model="fast";value.effort="low";
        result.resolve(std::move(value));return result;
      };
      entry.runForked=[&loop](const std::string& args,const Json&){
        event_loop::Deferred<user_input::InputResult> result(loop);
        user_input::InputResult value;
        value.messages.push_back(Json{{"type","user"},{"message",Json{{"content","fork:"+args}}}});
        value.resultText="forked";
        result.resolve(std::move(value));return result;
      };
      entries.push_back(std::move(entry));
    }
    user_input::SlashServices services{loop,messageServices,entries};
    services.pathExists=[&loop,test](const std::string&){
      event_loop::Deferred<bool> result(loop);
      if(value::property(test,"pathExists")==true)result.resolve(true);
      else result.reject(std::make_exception_ptr(std::runtime_error("ENOENT")));
      return result;
    };
    services.recordSkillUsage=[](const std::string&){};
    services.logError=[](const std::string&){};
    services.isFullscreen=[]{return false;};
    services.nowISOAfterMs=[](double){return "later";};
    services.buildPostCompactMessages=[](const Json& value){
      Json out=Json::array({Json{{"type","system"},{"subtype","compact_boundary"},
        {"isMeta",false}}});
      for(const auto& message:value.at("messagesToKeep"))out.push_back(message);
      return out;
    };
    services.resetMicrocompactState=[]{};
    auto pending=user_input::processSlashCommand(test.at("input").get<std::string>(),
      test.value("preceding",Json::array()),test.value("images",Json::array()),
      test.value("attachments",Json::array()),std::nullopt,services);
    Json result;
    pending.then([&](const user_input::InputResult* value,const std::exception_ptr& error){
      if(error){try{std::rethrow_exception(error);}catch(const std::exception& e){
        result=Json{{"error",e.what()}};}}
      else result=output(*value);
    });
    for(int i=0;i<20&&result.is_null();++i)loop.runReady();
    std::cout<<value::stringify(result)<<'\n';
  }
}
