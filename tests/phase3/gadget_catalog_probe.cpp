#include "gadgets/Catalog.hpp"
#include <cctype>
#include <iostream>
#include <string>
#include <vector>

using batbox::Json;namespace gadgets=batbox::gadgets;
static bool flag(const Json& t,const char* key){return t.value(key,false);}
static Json evaluate(const Json& t){
  if(t.at("op")=="parse"){
    const auto parsed=gadgets::parseGadgetPreset(t.at("preset").get<std::string>());
    return Json{{"result",parsed?Json(*parsed):Json()}};
  }
  std::vector<gadgets::Gadget> storage;storage.reserve(70);
  const auto make=[&](const std::string& name)->const gadgets::Gadget*{
    gadgets::Gadget def;def.name=name;def.isEnabled=[&t,name](){
      if(t.contains("disabled"))for(const auto& item:t.at("disabled"))if(item==name)return false;
      return true;};
    storage.push_back(gadgets::buildGadget(std::move(def)));return &storage.back();
  };
  gadgets::CatalogImports i;
  i.bat=make("Agent");i.missionOutput=make("TaskOutput");i.bash=make("Bash");
  i.glob=make("Glob");i.grep=make("Grep");i.exitPlanMode=make("ExitPlanMode");
  i.fileRead=make("Read");i.fileEdit=make("Edit");i.fileWrite=make("Write");
  i.notebookEdit=make("NotebookEdit");i.webFetch=make("Scrapling");
  i.todoWrite=make("TodoWrite");i.webSearch=make("SearXNG");
  i.missionStop=make("TaskStop");i.askUserQuestion=make("AskUserQuestion");
  i.skill=make("Skill");i.enterPlanMode=make("EnterPlanMode");
  i.config=make("Config");i.terminal=make("Terminal");i.suggestBackgroundPR=make("SuggestBackgroundPR");
  i.webBrowser=make("WebBrowser");i.missionCreate=make("TaskCreate");i.missionGet=make("TaskGet");
  i.missionUpdate=make("TaskUpdate");i.missionList=make("TaskList");
  i.overflowTest=make("OverflowTest");i.contextInspect=make("ContextInspect");
  i.terminalCapture=make("TerminalCapture");i.lsp=make("LSP");
  i.enterWorktree=make("EnterWorktree");i.exitWorktree=make("ExitWorktree");
  i.sendMessage=make("SendMessage");i.listPeers=make("ListPeers");
  i.teamCreate=make("TeamCreate");i.teamDelete=make("TeamDelete");
  i.verifyPlan=make("VerifyPlanExecution");i.repl=make("REPL");
  i.workflow=make("Workflow");i.sleep=make("Sleep");
  i.cronCreate=make("CronCreate");i.cronDelete=make("CronDelete");i.cronList=make("CronList");
  i.remoteTrigger=make("RemoteTrigger");i.monitor=make("Monitor");i.brief=make("Brief");
  i.sendUserFile=make("SendUserFile");i.pushNotification=make("PushNotification");
  i.subscribePR=make("SubscribePR");i.powerShell=make("PowerShell");i.snip=make("Snip");
  i.testingPermission=make("TestingPermission");i.listMcpResources=make("ListMcpResources");
  i.readMcpResource=make("ReadMcpResource");i.gadgetSearch=make("ToolSearch");
  gadgets::Gadgets mcp;
  if(t.contains("mcp"))for(const auto& item:t.at("mcp"))mcp.push_back(make(item.get<std::string>()));
  gadgets::CatalogSettings s;
  s.embeddedSearchGadgets=flag(t,"embedded");s.internalUser=flag(t,"internal");
  s.todoV2Enabled=flag(t,"todoV2");s.lspEnabled=flag(t,"lsp");
  s.worktreeModeEnabled=flag(t,"worktree");s.batSwarmsEnabled=flag(t,"swarm");
  s.simpleMode=flag(t,"simple");s.replModeEnabled=flag(t,"replMode");
  s.coordinatorModeEnabled=flag(t,"coordinator");s.gadgetSearchEnabledOptimistic=flag(t,"toolSearch");
  s.testEnvironment=flag(t,"test");s.powerShellEnabled=flag(t,"powerShell");
  s.cronEnabled=flag(t,"cron");s.remoteTriggerEnabled=flag(t,"remoteTrigger");
  s.monitorEnabled=flag(t,"monitor");s.webBrowserEnabled=flag(t,"webBrowser");
  s.workflowEnabled=flag(t,"workflow");s.historySnipEnabled=flag(t,"snip");
  s.overflowTestEnabled=flag(t,"overflow");s.contextInspectEnabled=flag(t,"contextInspect");
  s.terminalCaptureEnabled=flag(t,"terminalCapture");
  s.proactiveEnabled=flag(t,"proactive");s.kairosEnabled=flag(t,"kairos");
  s.kairosPushNotificationEnabled=flag(t,"pushNotification");
  s.kairosGithubWebhooksEnabled=flag(t,"subscribePR");
  s.verifyPlanEnabled=flag(t,"verifyPlan");s.udsInboxEnabled=flag(t,"listPeers");
  s.syntheticOutputGadgetName="SyntheticOutput";s.replGadgetName="REPL";
  if(t.contains("replOnly"))for(const auto& name:t.at("replOnly"))s.replOnlyGadgetNames.insert(name.get<std::string>());
  s.getDenyRuleForGadget=[&](const gadgets::Gadget& gadget,const Json&){
    if(t.contains("deny"))for(const auto& name:t.at("deny"))if(name==gadget.name)return true;
    return false;};
  s.localeCompare=[](const std::string& a,const std::string& b){
    std::string x=a,y=b;
    for(char& c:x)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for(char& c:y)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return x.compare(y);
  };
  s.getPowerShellGadget=[&](){return s.powerShellEnabled?i.powerShell:nullptr;};
  gadgets::Gadgets result;
  const std::string op=t.at("op").get<std::string>();
  if(op=="all")result=gadgets::getAllBaseGadgets(i,s);
  else if(op=="preset"){
    Json names=Json::array();for(const auto& name:gadgets::getGadgetsForDefaultPreset(i,s))names.push_back(name);
    return Json{{"result",names}};
  }else if(op=="tools")result=gadgets::getGadgets(Json::object(),i,s);
  else if(op=="pool")result=gadgets::assembleGadgetPool(Json::object(),mcp,i,s);
  else if(op=="merged")result=gadgets::getMergedGadgets(Json::object(),mcp,i,s);
  Json out=Json::array();
  for(const auto* gadget:result){bool isMcp=false;for(const auto* candidate:mcp)if(candidate==gadget){isMcp=true;break;}
    out.push_back(Json{{"name",gadget->name},{"source",isMcp?"mcp":"builtin"}});}
  return Json{{"result",out}};
}
int main(){std::string line;while(std::getline(std::cin,line)){
  try{std::cout<<batbox::value::stringify(evaluate(batbox::value::parse(line)))<<'\n';}
  catch(const std::exception& e){std::cout<<batbox::value::stringify(Json{{"error",e.what()}})<<'\n';}
}}
