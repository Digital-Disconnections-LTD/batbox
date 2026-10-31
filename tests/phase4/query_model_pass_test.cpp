#include "query/ModelPass.hpp"
#include "retry/Retry.hpp"
#include <cassert>
#include <iostream>

using namespace batbox;
static model::NextMessage stream(std::vector<Json> messages,bool fail=false){
  return [messages=std::move(messages),index=std::size_t{0},fail]() mutable
    -> std::optional<model::MessageRef>{
      if(index<messages.size())return std::make_shared<Json>(messages[index++]);
      if(fail)throw std::runtime_error("broken stream");
      return std::nullopt;
    };
}
static Json assistant(Json content,const char* id="m"){
  return Json{{"type","assistant"},{"uuid",std::string("u-")+id},
    {"message",{{"id",id},{"content",content}}}};
}
int main(){
  query::Params params;params.querySource="sdk";params.messages=Json::array();
  query::State state;
  query::PreparedIteration prepared;
  prepared.messagesForQuery=Json::array();prepared.fullSystemPoetry=Json::array();
  prepared.currentModel="main";prepared.queryTracking={{"chainId","chain"},{"depth",0}};
  prepared.appState={{"mcp",{{"clients",Json::array()},{"tools",Json::array()}}}};
  prepared.gadgetUseContext.data={{"options",{{"agentDefinitions",{{"activeAgents",Json::object()}}}}}};
  query::QueryConfig config;
  query::ModelServices services;
  services.feature=[](const std::string&){return false;};
  services.prependUserContext=[](const Json& messages,const Json&){return messages;};
  services.createUserMessage=[](const Json& args){return Json{{"type","user"},{"args",args}};};
  services.createAssistantAPIErrorMessage=[](const Json& args){return Json{{"type","assistant"},{"apiError","error"},{"args",args}};};
  services.createSystemMessage=[](const std::string& text,const std::string& level){
    return Json{{"type","system"},{"text",text},{"level",level}};
  };
  services.renderModelName=[](const std::string& model){return model;};
  Json emitted=Json::array();
  const Json text=assistant(Json::array({Json{{"type","text"},{"text","hello"}}}));
  services.callModel=[&](const query::ModelRequest& request,const std::function<void()>&)
    ->model::NextMessage{
    assert(request.options.at("model")=="main");return stream({text});
  };
  auto result=query::runModelPass(params,state,prepared,config,std::nullopt,services,
    [&](const Json& message){emitted.push_back(message);});
  assert(emitted.size()==1&&emitted[0]==text);
  assert(result.assistantMessages.size()==1&&!result.needsFollowUp);

  gadgets::Gadget gadget;gadget.name="Read";gadget.backfillObservableInput=[](Json& input){input["derived"]=7;};
  prepared.gadgetUseContext.gadgets={&gadget};
  const Json gadgetMessage=assistant(Json::array({Json{{"type","tool_use"},{"id","t1"},
    {"name","Read"},{"input",{{"path","a"}}}}}),"tool");
  services.callModel=[&](const query::ModelRequest&,const std::function<void()>&){return stream({gadgetMessage});};
  emitted=Json::array();
  result=query::runModelPass(params,state,prepared,config,std::nullopt,services,
    [&](const Json& message){emitted.push_back(message);});
  assert(result.needsFollowUp&&result.gadgetUseBlocks.size()==1);
  assert(emitted[0]["message"]["content"][0]["input"]["derived"]==7);
  assert(!result.assistantMessages[0]["message"]["content"][0]["input"].contains("derived"));

  params.fallbackModel="fallback";
  int attempts=0;
  services.callModel=[&](const query::ModelRequest& request,const std::function<void()>&)
    ->model::NextMessage{
    ++attempts;
    if(attempts==1){assert(request.options["model"]=="main");
      return [gadgetMessage,once=false]() mutable -> std::optional<model::MessageRef>{
        if(!once){once=true;return std::make_shared<Json>(gadgetMessage);}
        throw retry::FallbackTriggeredError("main","fallback");
      };}
    assert(request.options["model"]=="fallback");return stream({text});
  };
  emitted=Json::array();
  result=query::runModelPass(params,state,prepared,config,std::nullopt,services,
    [&](const Json& message){emitted.push_back(message);});
  assert(attempts==2&&emitted.size()==4);
  assert(emitted[0]["type"]=="assistant"&&emitted[1]["type"]=="user");
  assert(emitted[2]["type"]=="system"&&emitted[3]==text);
  assert(result.assistantMessages.size()==1&&!result.needsFollowUp);

  params.fallbackModel.reset();prepared.currentModel="main";
  const Json maxError={{"type","assistant"},{"apiError","max_output_tokens"},
    {"message",{{"content",Json::array()}}}};
  services.callModel=[&](const query::ModelRequest&,const std::function<void()>&){return stream({maxError});};
  emitted=Json::array();
  result=query::runModelPass(params,state,prepared,config,std::nullopt,services,
    [&](const Json& message){emitted.push_back(message);});
  assert(emitted.empty()&&result.assistantMessages.size()==1);

  services.callModel=[&](const query::ModelRequest&,const std::function<void()>&){return stream({gadgetMessage},true);};
  emitted=Json::array();
  result=query::runModelPass(params,state,prepared,config,std::nullopt,services,
    [&](const Json& message){emitted.push_back(message);});
  assert(result.terminal&&result.terminal->at("reason")=="model_error");
  assert(emitted.size()==3&&emitted[1]["type"]=="user"&&emitted[2]["apiError"]=="error");
  for(int backfillMode=0;backfillMode<3;++backfillMode){
    gadgets::Gadget observer;observer.name="Read";
    if(backfillMode==1)observer.backfillObservableInput=[](Json& input){input["path"]="expanded";};
    if(backfillMode==2)observer.backfillObservableInput=[](Json& input){input["derived"]=7;};
    prepared.gadgetUseContext.gadgets={&observer};
    auto original=std::make_shared<Json>(gadgetMessage);
    (*original)["message"]["usage"]={{"input_tokens",26}};
    (*original)["message"]["stop_reason"]=nullptr;
    services.callModel=[original](const query::ModelRequest&,const std::function<void()>&){
      return [original,index=0]() mutable -> std::optional<model::MessageRef>{
        if(index++==0)return original;
        if(index==2){
          (*original)["message"]["usage"]={{"input_tokens",42}};
          (*original)["message"]["stop_reason"]="end_turn";
          return std::make_shared<Json>(Json{{"type","stream_event"},
            {"event",{{"type","message_delta"},{"usage",{{"input_tokens",42}}},
              {"delta",{{"stop_reason","end_turn"}}}}}});
        }
        return std::nullopt;
      };
    };
    std::vector<query::MessageRef> refs;
    result=query::runModelPass(params,state,prepared,config,std::nullopt,services,
      query::Yield::withReferences([&](const query::MessageRef& ref){refs.push_back(ref);}));
    assert(refs.size()==2);
    assert(result.assistantMessages[0]["message"]["usage"]["input_tokens"]==42);
    if(backfillMode==2){
      assert(refs[0]!=original);
      assert((*refs[0])["message"]["usage"]["input_tokens"]==26);
      assert((*refs[0])["message"]["stop_reason"].is_null());
      assert((*refs[0])["message"]["content"][0]["input"]["derived"]==7);
    }else{
      assert(refs[0]==original);
      assert((*refs[0])["message"]["usage"]["input_tokens"]==42);
      assert((*refs[0])["message"]["stop_reason"]=="end_turn");
      assert((*refs[0])["message"]["content"][0]["input"]["path"]=="a");
    }
  }
  std::cout<<"model pass, backfill alias, fallback, withheld and failure paths passed\n";
}
