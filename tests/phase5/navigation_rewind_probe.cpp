#include "interactive/navigation/Rewind.hpp"
#include <iostream>

using namespace batbox;
using namespace batbox::interactive;
using namespace batbox::interactive::lifecycle;
using namespace batbox::interactive::navigation;
Json run(const std::string& kind){
  event_loop::Loop loop;
  std::vector<std::string> trace;
  auto target=std::make_shared<Json>(Json{{"type","user"},{"uuid","u1"},
    {"permissionMode","plan"},{"message",{{"content","restored"}}},
    {"imagePasteIds",Json::array({0,99})}});
  if(kind=="images")(*target)["message"]["content"]=Json::array({
    {{"type","text"},{"text","restored"}},
    {{"type","image"},{"source",{{"type","base64"},
      {"data","abc"},{"media_type","image/png"}}}},
    {{"type","image"},{"source",{{"type","url"},{"url","x"}}}}
  });
  auto other=std::make_shared<Json>(Json{{"type","assistant"},{"uuid","a1"}});
  auto initial=std::make_shared<TurnMessages>(kind=="missing"?
    TurnMessages{other}:TurnMessages{target,other,target,other});
  MessageServices messageServices;
  messageServices.publishMessages=[&](const TurnMessagesRef& value){
    std::string names;
    for(const auto& message:*value){
      if(!names.empty())names+=',';
      names+=(*message)["uuid"].get<std::string>();
    }
    trace.push_back("messages:"+names);
  };
  MessageState messages(initial,messageServices);
  auto app=std::make_shared<app_state::AppState>();
  app->fields=std::make_shared<value::SharedRecord>();
  app->permissionContext=std::make_shared<value::SharedRecord>(
    value::SharedRecord::fromJson({{"mode","default"}}));
  app->fields->set("toolPermissionContext",app->permissionContext->snapshot());
  app->fields->set("promptSuggestion",{{"text","old"}});
  auto store=std::make_shared<app_state::AppStateStore>(app,
    [&](const app_state::AppStateStore::Change& change){
      const auto mode=change.newState->permissionContext->get("mode");
      const auto suggestion=change.newState->fieldRef("promptSuggestion");
      trace.push_back("state:"+mode->get<std::string>()+":"+
        (suggestion->at("text").is_null()?"null":
          suggestion->at("text").get<std::string>()));
    });
  Json pasted={{"old",1}};
  RewindServices services(loop,messages,store);
  services.randomUUID=[&]{trace.push_back("uuid");return "new-id";};
  services.setConversationId=[&](const std::string& value){
    trace.push_back("conversation:"+value);
  };
  services.resetMicrocompactState=[&]{trace.push_back("micro");};
  services.resetContextCollapse=[&]{trace.push_back("collapse");};
  services.feature=[&](const std::string& name){
    return name=="CONTEXT_COLLAPSE"&&kind!="missing";
  };
  services.setInputValue=[&](const std::string& value){
    trace.push_back("input:"+value);
  };
  services.setInputMode=[&](const std::string& value){
    trace.push_back("mode:"+value);
  };
  services.setPastedContents=[&](const Json& value){
    pasted=value;
    std::string keys;
    for(const auto& key:value::objectKeys(value)){
      if(!keys.empty())keys+=',';
      keys+=key;
    }
    trace.push_back("pasted:"+keys);
  };
  services.publishSelectorVisible=[&](bool value){
    trace.push_back(std::string("selector:")+(value?"true":"false"));
  };
  auto rewind=std::make_shared<Rewind>(std::move(services));
  rewind->handleShowMessageSelector();rewind->handleShowMessageSelector();
  if(kind=="deferred"){
    rewind->handleRestoreMessage(target);
    trace.push_back("scheduled");
    trace.push_back("before:"+std::to_string(messages.messages()->size()));
    loop.runReady();
  }else rewind->restoreMessageSync(target);
  Json ids=Json::array();
  for(const auto& message:*messages.messages())ids.push_back((*message)["uuid"]);
  const auto state=store->getState();
  return Json{{"trace",trace},{"messageIds",ids},
    {"mode",state->permissionContext->get("mode")->get<std::string>()},
    {"suggestion",state->fieldRef("promptSuggestion")->at("text")},
    {"pasted",pasted},{"selector",rewind->selectorVisible()}};
}
int main(int argc,char** argv){
  if(argc>1&&std::string(argv[1])!="--probe")return 2;
  for(const auto& kind:{"ordinary","images","missing","deferred"})
    std::cout<<run(kind).dump()<<'\n';
}
