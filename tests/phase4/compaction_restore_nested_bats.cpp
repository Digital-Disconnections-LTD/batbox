#include "compaction/Restore.hpp"
#include "normalization/Attachments.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

using namespace batbox;
using namespace batbox::compaction;
namespace {
void check(bool value,const std::string& message){
  if(!value)throw std::logic_error(message);
}
Json missions(){
  const auto bat=[](const std::string& id,const std::string& description,
      const std::string& status){
    return Json{{"type","local_agent"},{"agentId",id},{"description",description},
      {"status",status},{"retrieved",false},{"progress",{{"summary","Reading"}}}};
  };
  Json tasks=Json::object();
  tasks["bat-audit"]=bat("bat-audit","Audit the parser","running");
  tasks["bat-lexer"]=bat("bat-lexer","Check the lexer","running");
  tasks["bat-lexer"]["parentBatId"]="bat-audit";
  tasks["bat-tokens"]=bat("bat-tokens","Count the tokens","completed");
  tasks["bat-tokens"]["parentBatId"]="bat-lexer";
  return tasks;
}
std::vector<Json> restore(const Json& agent){
  RestoreServices services;
  services.createAttachmentMessage=[](const Json& value){return value;};
  services.getMissionOutputPath=[](const std::string& id){return "/tmp/"+id+".output";};
  Context context;context.data=Json::object();
  if(!agent.is_null())context.data["agentId"]=agent;
  const auto tasks=missions();
  context.getAppState=[tasks]{return Json{{"tasks",tasks}};};
  const auto result=createAsyncBatAttachmentsIfNeeded(context,services);
  return std::vector<Json>(result.begin(),result.end());
}
const Json& find(const std::vector<Json>& attachments,const std::string& id){
  for(const auto& attachment:attachments)if(attachment.value("taskId","")==id)return attachment;
  throw std::logic_error("No attachment for "+id);
}
std::string text(const Json& attachment){
  messages::Services messages;int counter=0;
  messages.randomUUID=[&counter]{return "uuid-"+std::to_string(++counter);};
  messages.nowISO=[]{return std::string("2026-11-02T00:00:00.000Z");};
  normalization::AttachmentServices services;
  services.messageServices=&messages;
  services.sendMessageGadgetName="SendMessage";
  services.missionOutputGadgetName="TaskOutput";
  const auto normalized=normalization::normalizeMoreAttachmentForAPI(attachment,services);
  check(normalized&&normalized->is_array()&&!normalized->empty(),"task_status must normalize");
  return (*normalized)[0].dump();
}
}
int main(){
  try{
    // The main Flight compacts: every running Bat is announced again, and a
    // sub-Bat says which Bat launched it and where its result goes.
    const auto leader=restore(Json());
    check(leader.size()==3,"Every Bat in the tree must be re-announced");
    check(!find(leader,"bat-audit").contains("launchedBy"),"A top-level Bat has no launcher");
    const auto& lexer=find(leader,"bat-lexer");
    check(lexer.value("launchedBy","")=="Audit the parser"&&lexer.value("status","")=="running",
      "A running sub-Bat must be announced with its launcher: "+lexer.dump());
    const auto announced=text(lexer);
    check(announced.find("Agent \\\"Check the lexer\\\" (bat-lexer), launched by the Bat "
      "\\\"Audit the parser\\\", is still running")!=std::string::npos&&
      announced.find("Its result goes to that Bat, not to you")!=std::string::npos,
      "The leader must not expect a sub-Bat's notification: "+announced);
    const auto settled=text(find(leader,"bat-tokens"));
    check(settled.find("(launched by the Bat \\\"Check the lexer\\\")")!=std::string::npos,
      "A settled sub-Bat names its launcher: "+settled);
    const auto top=text(find(leader,"bat-audit"));
    check(top.find("Background agent \\\"Audit the parser\\\" (bat-audit) is still running")!=
      std::string::npos,"Top-level Bats keep their announcement: "+top);
    // The launching Bat itself hears about its own sub-Bat the usual way.
    const auto parent=restore("bat-audit");
    check(parent.size()==2&&!find(parent,"bat-lexer").contains("launchedBy"),
      "A Bat's own sub-Bat is announced to it without a launcher note");
    std::cout<<"Running sub-Bats are re-announced after compaction with the Bat that launched them\n";
    return 0;
  }catch(const std::exception& error){
    std::cerr<<error.what()<<"\n";return 1;
  }
}
