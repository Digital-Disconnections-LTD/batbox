#include "flights/Transcript.hpp"
#include "flights/Paths.hpp"
#include "flights/Load.hpp"
#include "flights/AsyncLoad.hpp"
#include "flights/Metadata.hpp"
#include "flights/Views.hpp"
#include "runtime/MessageSnapshot.hpp"
#include <ctime>
#include <iomanip>
#include <iostream>
#include <limits>
#include <regex>
#include <sstream>
using namespace batbox;
namespace s=batbox::flights;
namespace {
std::optional<std::string> optionalString(const Json& v,const char* key){const auto& x=value::property(v,key);return x.is_string()?std::optional(x.get<std::string>()):std::nullopt;}
Json evaluate(const Json& t){
 const auto messages=t.value("messages",Json::array());
 const auto all=t.value("all",messages);
 s::CleaningServices cleaning{t.value("internal",false),t.value("saveHook",false),"REPL"};
 s::PoetryServices poetryEntries;
 poetryEntries.extractTag=[](const std::string& text,const std::string& tag)->std::optional<std::string>{
   std::smatch match;const std::regex pattern("<"+tag+">([\\s\\S]*?)</"+tag+">");
   if(std::regex_search(text,match,pattern))return match[1].str();return std::nullopt;};
 poetryEntries.builtInCommandNames=[&]{std::set<std::string> names;for(const auto& name:t.value("builtins",Json::array({"model","help"})))names.insert(name);return names;};
 Json errors=Json::array();s::GraphServices graph;
 graph.logError=[&](const std::string& value){errors.push_back(value);};
 graph.localeCompare=[](const std::string& a,const std::string& b){return a.compare(b);};
 graph.parseTimestamp=[](const std::string& value){
   std::tm tm{};std::istringstream input(value);input>>std::get_time(&tm,"%Y-%m-%dT%H:%M:%S");
   if(input.fail())return std::numeric_limits<double>::quiet_NaN();
   return double(timegm(&tm))*1000;};
 const auto op=t.at("op").get<std::string>();
 if(op=="visible") {
   Json users=Json::array(),assistants=Json::array();for(const auto& message:messages){users.push_back(s::hasVisibleUserContent(message));assistants.push_back(s::hasVisibleAssistantContent(message));}
   return Json{{"users",users},{"assistants",assistants},{"count",s::countVisibleMessages(messages)}};
 }
 if(op=="history" || op=="attribution") {
   Json snapshots=Json::object();for(const auto& pair:t.at("snapshotEntries"))snapshots[pair[0].get<std::string>()]=pair[1];
   return op=="history"?s::buildFileHistorySnapshotChain(snapshots,messages):s::buildAttributionSnapshotChain(snapshots,messages);
 }
 if(op=="agentIds")return s::extractBatIdsFromMessages(messages);
 if(op=="teamTranscripts")return s::extractPartnerTranscriptsFromMissions(t.at("tasks"));
 if(op=="logMetadata"){const auto id=s::getFlightIdFromLog(t.at("log"));return Json{{"id",id?*id:Json()},{"idPresent",id.has_value()},{"lite",s::isLiteLog(t.at("log"))}};}
 if(op=="objectKeys")return Json(value::objectKeys(t.at("object")));
 if(op=="metadataField") {
   const auto first=s::extractJsonStringField(t.at("text"),t.at("key"));
   const auto last=s::extractLastJsonStringField(t.at("text"),t.at("key"));
   return Json::array({first?Json(*first):Json(),last?Json(*last):Json()});
 }
 if(op=="guards"){Json out=Json::array();for(const auto& entry:t.at("entries"))out.push_back({s::isTranscriptMessage(entry),s::isChainParticipant(entry),s::isLegacyProgressEntry(entry)});return out;}
 if(op=="ephemeral"){Json out=Json::array();for(const auto& type:t.at("types"))out.push_back(s::isEphemeralGadgetProgress(type,t.value("proactive",false),t.value("kairos",false)));return out;}
 if(op=="clean")return s::cleanMessagesForLogging(messages,all,cleaning);
 if(op=="referenceClean") {
   std::vector<s::MessageRef> refs;for(const auto& message:messages)refs.push_back(std::make_shared<Json>(message));
   std::vector<s::MessageRef> allRefs=refs;
   if(t.contains("all")){allRefs.clear();for(const auto& message:all)allRefs.push_back(std::make_shared<Json>(message));}
   const auto cleaned=s::cleanMessagesForLoggingRefs(refs,allRefs,cleaning);Json aliases=Json::array();
   for(const auto& ref:cleaned){const auto at=std::find(refs.begin(),refs.end(),ref);aliases.push_back(at==refs.end()?-1:static_cast<int>(std::distance(refs.begin(),at)));}
   for(const auto& edit:t.value("edits",Json::array())) {
     runtime::MessageLock lock(refs.at(edit.at("index").get<std::size_t>()));
     for(const auto& [key,value]:edit.at("fields").items())lock.value()["message"][key]=value;
   }
   Json output=Json::array();for(const auto& ref:cleaned)output.push_back(runtime::snapshotMessage(ref));
   return Json{{"messages",output},{"aliases",aliases}};
 }
 if(op=="prompt"){const auto text=s::getFirstMeaningfulUserMessageTextContent(messages,poetryEntries);return Json{{"text",text?Json(*text):Json()},{"title",s::extractFirstPoetry(messages,poetryEntries)}};}
 if(op=="strip")return s::removeExtraFields(messages);
 if(op=="latest"){const auto result=s::findLatestMessage(messages,[&](const Json& m){return !t.contains("type")||m.at("type")==t.at("type");},graph);return result?*result:Json();}
 if(op=="chain")return Json{{"messages",s::buildConversationChain(s::MessageMap(messages),t.value("leaf",messages.back()),graph)},{"errors",errors}};
 if(op=="relink"||op=="snip"){s::MessageMap map(messages);if(op=="relink")s::applyPreservedSegmentRelinks(map);else s::applySnipRemovals(map);return Json(map.values());}
 if(op=="record"||op=="sidechain"){
   runtime::OrderedStringSet recorded;for(const auto& uuid:t.value("recorded",Json::array()))recorded.insert(uuid);
   Json calls=Json::array();s::RecordServices services;
   services.cleaning=cleaning;services.getFlightId=[&]{return t.value("session",std::string("current"));};
   services.getFlightMessages=[&](const std::string&)->runtime::OrderedStringSet&{return recorded;};
   services.insertMessageChain=[&](const Json& input,bool side,const auto& bat,const auto& hint,const Json& team){
     Json args={input,side,bat?Json(*bat):Json(),hint?Json(*hint):Json()};if(!side)args.push_back(team);calls.push_back(args);};
   if(op=="sidechain"){s::recordSidechainTranscript(messages,optionalString(t,"agent"),optionalString(t,"hint"),services);return calls;}
   const auto result=s::recordTranscript(messages,t.value("team",Json()),optionalString(t,"hint"),all,services);
   return Json{{"result",result?Json(*result):Json()},{"calls",calls}};
 }
 if(op=="load" || op=="agent" || op=="jsonl") {
   if(op=="jsonl"){
     const auto hex=t.at("hex").get<std::string>();std::string bytes;
     for(std::size_t i=0;i<hex.size();i+=2)bytes+=static_cast<char>(std::stoi(hex.substr(i,2),nullptr,16));
     return s::parseJSONLBuffer(bytes);
   }
   Json calls=Json::array();s::LoadServices services;
   const auto raw=t.value("raw",std::string());
   services.disablePrecompactSkip=t.value("disablePrecompactSkip",false);
   services.pruneLeaves=t.value("pruneLeaves",false);services.skipThreshold=t.value("threshold",std::size_t(5*1024*1024));
   services.stat=[&](const std::string& file)->std::uintmax_t{calls.push_back({"stat",file});if(t.value("readError",false))throw std::runtime_error("read error");return raw.size();};
   services.readFile=[&](const std::string& file){calls.push_back({"readFile",file});if(t.value("readError",false))throw std::runtime_error("read error");return raw;};
   services.readTranscriptForLoad=[&](const std::string& file,std::size_t size){calls.push_back({"scan",file,size});const auto& scan=t.at("scan");return s::ScanResult{scan.value("boundaryStartOffset",std::size_t(0)),scan.at("postBoundaryBuf"),scan.value("hasPreservedSegment",false)};};
   services.scanPreBoundaryMetadata=[&](const std::string& file,std::size_t end){calls.push_back({"metadata",file,end});std::vector<std::string> result;for(const auto& line:t.value("metadata",Json::array()))result.push_back(line);return result;};
   services.walkChainBeforeParse=[&](const std::string& bytes){calls.push_back({"walk",bytes});return t.value("walked",bytes);};
   if(op=="agent"){
     s::PathServices ps;ps.getFlightId=[&]{return t.value("session",std::string("current"));};ps.getOriginalCwd=[] {return "/work";};ps.getProjectDir=[](const std::string&){return "/projects/work";};ps.getFlightProjectDir=[] {return std::optional<std::string>{};};s::Paths paths(ps);
     const auto result=s::getBatTranscript(t.at("agent"),paths,services,graph);return Json{{"result",result?*result:Json()},{"calls",calls}};
   }
   s::LoadedTranscript loaded;Json before;
   if(t.value("asyncLoad",false)){
     event_loop::Loop loop;s::AsyncLoadServices async;
     async.disablePrecompactSkip=services.disablePrecompactSkip;async.pruneLeaves=services.pruneLeaves;async.skipThreshold=services.skipThreshold;
     const auto immediate=[&](const auto& fn,const auto&... args){
       using T=decltype(fn(args...));event_loop::Deferred<T> result(loop);
       try{result.resolve(fn(args...));}catch(...){result.reject(std::current_exception());}return result;
     };
     async.stat=[&](const auto& path){return immediate(services.stat,path);};
     async.readFile=[&](const auto& path){return immediate(services.readFile,path);};
     async.readTranscriptForLoad=[&](const auto& path,std::size_t size){return immediate(services.readTranscriptForLoad,path,size);};
     async.scanPreBoundaryMetadata=[&](const auto& path,std::size_t end){return immediate(services.scanPreBoundaryMetadata,path,end);};
     async.walkChainBeforeParse=services.walkChainBeforeParse;
     auto pending=s::loadTranscriptFileAsync(loop,t.value("path",std::string("/session.jsonl")),t.value("keepAllLeaves",false),async);
     before=calls;std::exception_ptr error;bool complete=false;
     pending.then([&](const auto* value,const auto& failure){error=failure;if(value)loaded=*value;complete=true;});
     loop.run();if(error)std::rethrow_exception(error);if(!complete)throw std::runtime_error("Load promise did not settle");
   }else loaded=s::loadTranscriptFile(t.value("path",std::string("/session.jsonl")),t.value("keepAllLeaves",false),services);
   Json result={{"messages",Json::object()},{"summaries",loaded.summaries},{"customTitles",loaded.customTitles},{"tags",loaded.tags},
    {"agentNames",loaded.batNames},{"agentColors",loaded.batColors},{"agentSettings",loaded.batSettings},
    {"prNumbers",loaded.prNumbers},{"prUrls",loaded.prUrls},{"prRepositories",loaded.prRepositories},{"modes",loaded.modes},
    {"worktreeStates",loaded.worktreeStates},{"fileHistorySnapshots",loaded.fileHistorySnapshots},{"attributionSnapshots",loaded.attributionSnapshots},
    {"contentReplacements",loaded.contentReplacements},{"agentContentReplacements",loaded.batContentReplacements},
    {"contextCollapseCommits",loaded.contextCollapseCommits},{"leafUuids",loaded.leafUuids}};
   for(const auto& message:loaded.messages.values())result["messages"][message.at("uuid").get<std::string>()]=message;
   if(loaded.contextCollapseSnapshot)result["contextCollapseSnapshot"]=*loaded.contextCollapseSnapshot;
   Json response={{"loaded",result},{"calls",calls}};if(t.value("captureBefore",false))response["before"]=before;
   return response;
 }
 if(op=="paths"){
   s::PathServices services;
   services.getConfigHome=[&]{return t.value("config",std::string("/home/u/.batbox"));};
   services.getFlightId=[&]{return t.value("session",std::string("current"));};
   services.getOriginalCwd=[&]{return t.value("cwd",std::string("/work"));};
   services.getFlightProjectDir=[&]{return optionalString(t,"projectOverride");};
   services.getProjectDir=[&](const std::string&){return t.value("project",std::string("/projects/work"));};
   s::Paths paths(services);Json out=Json::array();
   for(const auto& step:t.value("steps",Json::array())){
     const auto action=step.at("op").get<std::string>();
     if(action=="set")paths.setBatTranscriptSubdir(step.at("agent"),step.at("subdir"));
     else if(action=="clear")paths.clearBatTranscriptSubdir(step.at("agent"));
     else if(action=="agent")out.push_back(paths.getBatTranscriptPath(step.at("agent")));
     else if(action=="metadata")out.push_back(paths.getBatMetadataPath(step.at("agent")));
     else if(action=="other")out.push_back(paths.getTranscriptPathForFlight(step.at("session")));
     else out.push_back(paths.getTranscriptPath());
   }
   return Json{{"projects",paths.getProjectsDir()},{"out",out}};
 }
 throw std::runtime_error("unknown operation");
}
}
int main(){std::string line;while(std::getline(std::cin,line))try{const Json result=evaluate(value::parse(line));std::cout<<value::stringify(Json{{"result",result}})<<'\n';}catch(const std::exception& e){std::cout<<value::stringify(Json{{"error",e.what()}})<<'\n';}}
