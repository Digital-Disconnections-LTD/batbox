#include "normalization/Attachments.hpp"
#include <iostream>
#include <string>

using batbox::Json;
namespace norm=batbox::normalization;
static Json evaluate(const Json& t){
  int uuidCounter=0;
  batbox::messages::Services msg;
  msg.randomUUID=[&uuidCounter](){return "fixture-uuid-"+std::to_string(++uuidCounter);};
  msg.nowISO=[](){return "2026-10-28T00:00:00.000Z";};
  norm::AttachmentServices services;
  services.messageServices=&msg;
  services.jsonStringify=[](const Json& value){return value.dump(-1,' ',false,Json::error_handler_t::replace);};
  services.shellQuote=[](const Json& args){
    std::string value=args.at(0).get<std::string>();std::string result="'";
    for(char c:value)result+=c=='\''?"'\\''":std::string(1,c);
    return result+"'";
  };
  services.mapBashResult=[](const Json& output,const std::string&){return Json{{"content",output.at("stdout")}};};
  services.mapFileReadResult=[](const Json& output,const std::string&){
    return Json{{"content",output.contains("resultContent")?output.at("resultContent"):output.at("content")}};
  };
  services.formatPartnerMessages=[](const Json& messages){return "Mailbox: "+std::to_string(messages.size())+" messages";};
  services.formatNumber=[](double n){return batbox::value::stringify(Json(n));};
  services.formatFileSize=[](double n){
    auto one=[](double x,const std::string& unit){
      std::string s=std::to_string(x);s=s.substr(0,s.find('.')+2);
      if(s.size()>=2&&s.substr(s.size()-2)==".0")s.resize(s.size()-2);
      return s+unit;
    };
    const double kb=n/1024;if(kb<1)return batbox::value::stringify(Json(n))+" bytes";
    if(kb<1024)return one(kb,"KB");
    const double mb=kb/1024;
    if(mb<1024)return one(mb,"MB");
    return one(mb/1024,"GB");
  };
  services.companionIntroText=[](const std::string& name,const std::string& species){return "Companion "+name+" ("+species+")";};
  services.memoryHeader=[](const std::string& path,const Json& mtime){return "Header "+path+" "+batbox::value::stringify(mtime);};
  services.missionCreateGadgetName="TaskCreate";services.missionUpdateGadgetName="TaskUpdate";
  services.missionOutputGadgetName="TaskOutput";services.sendMessageGadgetName="SendMessage";
  services.batGadgetName="Agent";services.fileReadGadgetName="Read";
  services.bashGadgetName="Bash";services.fileEditGadgetName="Edit";services.fileWriteGadgetName="Write";
  services.exploreBatType="Explore";services.planBatType="Plan";
  services.askUserQuestionGadgetName="AskUserQuestion";services.globGadgetName="Glob";services.grepGadgetName="Grep";
  services.verifyPlanGadgetName="VerifyPlanExecution";
  services.exitPlanModeGadgetName="ExitPlanMode";
  services.snipNudgeText="Trim stale context.";
  services.formatDiagnosticsSummary=[](const Json& files){return "Diagnostics: "+std::to_string(files.size());};
  services.outputStyleConfig=t.value("outputStyleConfig",Json::object());
  services.todoV2Enabled=t.value("todoV2Enabled",false);
  services.historySnipFeature=t.value("historySnipFeature",false);
  services.verifyPlanEnabled=t.value("verifyPlanEnabled",false);
  services.batSwarmsEnabled=t.value("agentSwarmsEnabled",false);
  services.skillSearchEnabled=t.value("skillSearchEnabled",false);
  services.planInterviewEnabled=t.value("planInterviewEnabled",false);
  services.explorePlanBatsEnabled=t.value("explorePlanAgentsEnabled",false);
  services.embeddedSearchGadgets=t.value("embeddedSearchTools",false);
  services.allowedGadgets=t.value("allowedTools",Json());
  services.planBatCount=t.value("planAgentCount",1);
  services.exploreBatCount=t.value("exploreAgentCount",1);
  if(t.contains("planPhase4Variant")&&t.at("planPhase4Variant").is_string())
    services.planPhase4Variant=t.at("planPhase4Variant").get<std::string>();
  return norm::normalizeSimpleAttachmentForAPI(t.at("attachment"),services);
}
int main(){std::string line;while(std::getline(std::cin,line)){
  try{std::cout<<batbox::value::stringify(Json{{"result",evaluate(batbox::value::parse(line))}})<<'\n';}
  catch(const std::exception& error){std::cout<<batbox::value::stringify(Json{{"error",error.what()}})<<'\n';}
}}
