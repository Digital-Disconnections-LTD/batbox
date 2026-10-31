#include "interactive/navigation/Rewind.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
using namespace batbox::interactive;
using namespace batbox::interactive::lifecycle;
using namespace batbox::interactive::navigation;
Json run(const std::string& kind){
  event_loop::Loop loop;
  std::vector<std::string> trace;
  auto raw=std::make_shared<Json>(Json{{"uuid","123456789012345678901234-source"},
    {"type","user"},{"message",{{"content","hello"}}}});
  auto other=std::make_shared<Json>(Json{{"uuid","after"},
    {"type","assistant"}});
  auto displayed=std::make_shared<TurnMessages>(kind=="missing"?
    TurnMessages{other}:TurnMessages{raw,other});
  MessageServices messageServices;
  messageServices.publishMessages=[&](const TurnMessagesRef& value){
    if(value->empty())trace.push_back("restore:true");
  };
  MessageState messages(displayed,messageServices);
  auto app=std::make_shared<app_state::AppState>();
  app->fields=std::make_shared<value::SharedRecord>();
  app->fields->set("fileHistory",{{"id","history"}});
  const auto history=app->fieldRef("fileHistory");
  app->permissionContext=std::make_shared<value::SharedRecord>(
    value::SharedRecord::fromJson({{"mode","default"}}));
  app->fields->set("toolPermissionContext",app->permissionContext->snapshot());
  auto store=std::make_shared<app_state::AppStateStore>(app);
  RewindServices services(loop,messages,store);
  services.selectableUserMessage=[&](const TurnMessageRef& value){
    trace.push_back(std::string("selectable:")+(value==raw?"true":"false"));
    return kind!="unselectable";
  };
  services.fileHistoryHasAnyChanges=[&](value::ValueRef value,
      const std::string& uuid){
    trace.push_back(std::string("history:")+(value==history?"true":"false")+
      ":"+uuid);
    event_loop::Deferred<bool> result(loop);
    if(kind=="history-error")result.reject(std::make_exception_ptr(
      std::runtime_error("history failed")));
    else result.resolve(kind=="changed");
    return result;
  };
  services.messagesAfterAreOnlySynthetic=[&](const TurnMessagesRef& value,
      std::size_t index){
    trace.push_back(std::string("synthetic:")+
      (value==displayed?"true":"false")+":"+std::to_string(index));
    return kind!="nonsynthetic";
  };
  services.onCancel=[&]{trace.push_back("cancel");};
  services.publishPreselect=[&](TurnMessageRef value){
    trace.push_back(std::string("preselect:")+(value==raw?"true":"false"));
  };
  services.publishSelectorVisible=[&](bool value){
    trace.push_back(std::string("visible:")+(value?"true":"false"));
  };
  services.randomUUID=[]{return "new";};
  services.setConversationId=[](const std::string&){};
  services.resetMicrocompactState=[]{};
  services.feature=[](const std::string&){return false;};
  services.setInputValue=[](const std::string&){};
  services.setInputMode=[](const std::string&){};
  auto rewind=std::make_shared<Rewind>(std::move(services));
  auto result=rewind->editMessage("123456789012345678901234-render",
    displayed,history);
  result.then([&](const event_loop::Unit*,const std::exception_ptr& error){
    if(error)try{std::rethrow_exception(error);}
      catch(const std::exception& value){
        trace.push_back(std::string("rejected:")+value.what());
      }
  });
  for(int index=0;index<5;++index)loop.runReady();
  return Json{{"trace",trace},{"preselect",rewind->preselect()==raw},
    {"visible",rewind->selectorVisible()}};
}
int main(int argc,char** argv){
  if(argc>1&&std::string(argv[1])!="--probe")return 2;
  for(const auto& kind:{"missing","unselectable","direct","changed",
      "nonsynthetic","history-error"})std::cout<<run(kind).dump()<<'\n';
}
