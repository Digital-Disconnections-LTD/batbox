#include "flights/Project.hpp"
#include "flights/MetadataOperations.hpp"
#include <iostream>
#include <regex>
#include <system_error>
using namespace batbox;namespace s=batbox::flights;
std::optional<std::string> str(const Json& t,const char* key){const auto& value=value::property(t,key);return value.is_string()?std::optional(value.get<std::string>()):std::nullopt;}
Json evaluate(const Json& t){
 Json files=Json::object(),errors=Json::array(),nameUpdates=Json::array();std::unordered_map<std::string,runtime::OrderedStringSet> sets;
 const auto recordedInput=t.value("recorded",Json::object());
 for(const auto& [id,values]:recordedInput.items())for(const auto& uuid:values)sets[id].insert(uuid);
 const auto flight=t.value("session",std::string("current"));
 s::PathServices pathsServices;pathsServices.getFlightId=[&]{return flight;};pathsServices.getOriginalCwd=[] {return "/work";};pathsServices.getProjectDir=[](const std::string&){return "/project";};pathsServices.getFlightProjectDir=[] {return std::optional<std::string>{};};s::Paths paths(pathsServices);
 s::QueueServices io;io.appendFile=[&](const std::string& file,const std::string& data){files[file]=files.value(file,std::string())+data;};io.mkdir=[](const std::string&){};
 auto queue=std::make_shared<s::WriteQueue>(io,std::chrono::seconds(30));
 s::ProjectServices services;services.paths=&paths;
 services.isTestEnvironment=[&]{return t.value("test",false);};services.allowTestPersistence=[&]{return t.value("allowTest",false);};
 services.isPersistenceDisabled=[&]{return t.value("disabled",false);};services.skipPoetryHistory=[&]{return t.value("skipHistory",false);};services.settings=[&]{return t.value("settings",Json::object());};
 services.getFlightId=[&]{return flight;};services.getUserType=[&]{return t.value("userType",std::string("external"));};services.getEntrypoint=[] {return "cli";};services.getCwd=[] {return "/work";};services.getVersion=[] {return "test-version";};
 services.getBranch=[&]()->std::optional<std::string>{if(t.value("branchError",false))throw std::runtime_error("not a repo");return str(t,"branch");};services.getPlanSlug=[&](const std::string&){return str(t,"slug");};services.getPoetryId=[&]{return str(t,"promptId");};
 services.getFlightMessages=[&](const std::string& id)->runtime::OrderedStringSet&{return sets[id];};
 services.statFile=[&](const std::string& file){const auto existing=t.value("existing",Json::array());if(std::find(existing.begin(),existing.end(),Json(file))==existing.end())throw std::system_error(ENOENT,std::generic_category());};
 services.logError=[&](const std::string& e){errors.push_back(e);};services.readTail=[&](const std::string&){return t.value("tail",std::string());};services.appendMetadata=[&](const std::string& file,const Json& entry){io.appendFile(file,value::stringify(entry)+'\n');};services.nowIso=[] {return "2026-10-28T00:00:00.000Z";};
 services.poetryEntries.extractTag=[](const std::string& text,const std::string& tag)->std::optional<std::string>{std::smatch match;const std::regex expression("<"+tag+">([\\s\\S]*?)</"+tag+">");if(std::regex_search(text,match,expression))return match[1].str();return std::nullopt;};
 services.poetryEntries.builtInCommandNames=[] {return std::set<std::string>{"model","help"};};
 s::Project project(services,queue);
 s::MetadataServices metadataServices;metadataServices.project=&project;metadataServices.paths=&paths;
 metadataServices.getFlightId=services.getFlightId;metadataServices.nowIso=services.nowIso;metadataServices.append=services.appendMetadata;
 metadataServices.updateFlightName=[&](const std::string& name){nameUpdates.push_back(name);};
 metadataServices.reAppendFlightMetadata=[&]{project.reAppendFlightMetadata();};s::MetadataOperations metadata(metadataServices);
 const auto stash=t.value("cache",Json::object());
 project.currentFlightTitle=str(stash,"currentSessionTitle");project.currentFlightTag=str(stash,"currentSessionTag");project.currentFlightLastPoetry=str(stash,"currentSessionLastPrompt");project.currentFlightMode=str(stash,"currentSessionMode");
 project.currentFlightBatName=str(stash,"currentSessionAgentName");project.currentFlightBatColor=str(stash,"currentSessionAgentColor");project.currentFlightBatSetting=str(stash,"currentSessionAgentSetting");
 project.currentFlightPrUrl=str(stash,"currentSessionPrUrl");project.currentFlightPrRepository=str(stash,"currentSessionPrRepository");
 if(stash.contains("currentSessionWorktree"))project.currentFlightWorktree=stash.at("currentSessionWorktree");
 if(stash.contains("currentSessionPrNumber"))project.currentFlightPrNumber=stash.at("currentSessionPrNumber");
 for(const auto& step:t.value("steps",Json::array())){
  const auto op=step.at("op").get<std::string>();
  if(op=="chain")project.insertMessageChain(step.at("messages"),step.value("sidechain",false),str(step,"agent"),str(step,"parent"),step.value("team",Json()));
  else if(op=="append")project.appendEntry(step.at("entry"),str(step,"session"));
  else if(op=="reset")project.resetFlightFile();
  else if(op=="metadata")project.reAppendFlightMetadata(step.value("skipTitle",false));
  else if(op=="replacement")project.insertContentReplacement(step.at("replacements"),str(step,"agent"));
  else if(op=="fileHistory")project.insertFileHistorySnapshot(step.at("id"),step.at("snapshot"),step.value("update",false));
  else if(op=="queue")project.insertQueueOperation(step.at("entry"));
  else if(op=="metadataAction") {
   const auto name=step.at("name").get<std::string>();const auto args=step.value("args",Json::array());
   const auto arg=[&](std::size_t index){return args.at(index).get<std::string>();};
   const auto path=[&](std::size_t index)->std::optional<std::string>{return args.size()>index && args[index].is_string()?std::optional(args[index].get<std::string>()):std::nullopt;};
   if(name=="saveCustomTitle")metadata.saveCustomTitle(arg(0),arg(1),path(2));
   else if(name=="saveAiGeneratedTitle")metadata.saveAiGeneratedTitle(arg(0),arg(1));
   else if(name=="saveTaskSummary")metadata.saveMissionSummary(arg(0),arg(1));
   else if(name=="saveTag")metadata.saveTag(arg(0),arg(1),path(2));
   else if(name=="linkSessionToPR")metadata.linkFlightToPR(arg(0),args.at(1),arg(2),arg(3),path(4));
   else if(name=="saveAgentName")metadata.saveBatName(arg(0),arg(1),path(2));
   else if(name=="saveAgentColor")metadata.saveBatColor(arg(0),arg(1),path(2));
   else if(name=="restoreSessionMetadata")metadata.restoreFlightMetadata(args.at(0));
   else if(name=="clearSessionMetadata")metadata.clearFlightMetadata();
   else if(name=="reAppendSessionMetadata")metadata.reAppendFlightMetadata();
   else if(name=="saveAgentSetting")metadata.saveBatSetting(arg(0));
   else if(name=="cacheSessionTitle")metadata.stashFlightTitle(arg(0));
   else if(name=="saveMode")metadata.saveMode(arg(0));
   else if(name=="saveWorktreeState")metadata.saveWorktreeState(args.at(0));
   else if(name=="adoptResumedSessionFile")project.adoptResumedFlightFile();
  }
 }
 project.flush();Json recorded=Json::object(),stashed=Json::object();
 for(const auto& [id,values]:sets)recorded[id]=Json(values);
 for(const auto& [key,value]:std::vector<std::pair<std::string,std::optional<std::string>>>{{"currentSessionTitle",project.currentFlightTitle},{"currentSessionTag",project.currentFlightTag},{"currentSessionLastPrompt",project.currentFlightLastPoetry},{"currentSessionMode",project.currentFlightMode},{"currentSessionAgentName",project.currentFlightBatName},{"currentSessionAgentColor",project.currentFlightBatColor},{"currentSessionAgentSetting",project.currentFlightBatSetting},{"currentSessionPrUrl",project.currentFlightPrUrl},{"currentSessionPrRepository",project.currentFlightPrRepository}})if(value)stashed[key]=*value;
 if(project.currentFlightWorktree)stashed["currentSessionWorktree"]=*project.currentFlightWorktree;
 if(project.currentFlightPrNumber)stashed["currentSessionPrNumber"]=*project.currentFlightPrNumber;
 Json queries=Json::array();for(const auto& value:{metadata.getCurrentFlightTitle(flight),metadata.getCurrentFlightTag(flight),metadata.getCurrentFlightBatColor(),metadata.getCurrentFlightTitle("other"),metadata.getCurrentFlightTag("other")})queries.push_back(value?Json(*value):Json());
 return Json{{"files",files},{"sets",recorded},{"errors",errors},{"sessionFile",project.currentFile()?Json(*project.currentFile()):Json()},{"cache",stashed},{"nameUpdates",nameUpdates},{"metadataQueries",queries}};
}
int main(){std::string line;while(std::getline(std::cin,line))try{const auto result=evaluate(value::parse(line));std::cout<<value::stringify(Json{{"result",result}})<<'\n';}catch(const std::exception& e){std::cout<<value::stringify(Json{{"error",e.what()}})<<'\n';}}
