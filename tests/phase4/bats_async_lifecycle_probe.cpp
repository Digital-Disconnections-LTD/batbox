#include "bats/AsyncLifecycle.hpp"
#include <iostream>
using batbox::Json;
namespace ba=batbox::bats;
namespace value=batbox::value;
static const Json& field(const Json& object,const char* key){return value::property(object,key);}
static bool flag(const Json& item,const char* key){return field(item,key)==true;}
static Json run(const Json& item){
 Json trace=Json::array(),state={{"tasks",{{"task",{{"type","local_agent"},
  {"retain",flag(item,"retain")},{"messages",Json::array()}}}}},
  {"toolPermissionContext",{{"mode","auto"}}}};
 ba::AsyncLifecycleParams params;
 params.missionId="task";params.description="description";
 params.batIdForCleanup="agent-id";
 params.abortOwner=std::make_shared<std::stop_source>();
 params.metadata={"prompt","model","worker",false,true,900};
 params.gadgetUseContext.query.data={{"toolUseId","tool-id"}};
 params.gadgetUseContext.getAppState=[&]{return state;};
 batbox::gadgets::Gadget read,bash;read.name="Read";bash.name="Bash";
 params.gadgetUseContext.query.gadgets={&read,&bash};
 params.rootSetAppState=[&](const std::function<Json(const Json&)>& updater){
  trace.push_back(Json::array({"setState"}));state=updater(state);};
 params.enableSummarization=flag(item,"summarize");
 params.makeStream=[&](const std::function<void(const std::shared_ptr<ba::StashSafeParams>&)>& stash,
    const batbox::query::Yield& yield){
  trace.push_back(Json::array({"stream",static_cast<bool>(stash)}));
  if(stash){auto snapshot=std::make_shared<ba::StashSafeParams>();
    snapshot->forkContextMessages=Json::array({Json{{"uuid","prefix"}}});stash(snapshot);}
  const Json& messages=field(item,"messages");
  if(messages.is_array())for(std::size_t i=0;i<messages.size();++i){
   trace.push_back(Json::array({"next",i}));
   auto ref=std::make_shared<Json>(messages[i]);yield(ref);
   if(flag(item,"lateMutation")&&field(*ref,"type")=="assistant"){
    (*ref)["message"]["usage"]={{"input_tokens",42},{"output_tokens",1}};
    (*ref)["message"]["stop_reason"]="end_turn";
   }
   const Json& fail=field(item,"failAfter");
   if(fail.is_number_integer()&&fail.get<int>()==static_cast<int>(i)){
    if(flag(item,"abort"))throw ba::BatAbortError{};
    throw std::runtime_error("stream failed");}}
  if(field(item,"failAfter")==-1){
   if(flag(item,"abort"))throw ba::BatAbortError{};
   throw std::runtime_error("stream failed");}
 };
 int worktreeCalls=0;
 params.getWorktreeResult=[&]{trace.push_back(Json::array({"worktree"}));
  if(flag(item,"worktreeFailure")&&++worktreeCalls==1)
   throw std::runtime_error("worktree failed");
  return Json{{"worktreePath","/worktree"}};};
 ba::AsyncLifecycleImports imports;
 imports.createProgressTracker=[&]{trace.push_back(Json::array({"tracker"}));
  return Json{{"tokenCount",0},{"toolUseCount",0}};};
 imports.createActivityDescriptionResolver=[&](const batbox::gadgets::Gadgets& gadgets){
  Json names=Json::array();for(auto* gadget:gadgets)names.push_back(gadget->name);
  trace.push_back(Json::array({"resolver",names}));
  return ba::ActivityResolver([](const std::string&,const Json&){
   return std::optional<std::string>("working");});};
 imports.startBatSummarization=[&](const std::string& missionId,
    const std::shared_ptr<ba::StashSafeParams>& snapshot,const ba::RootSetAppState&){
  trace.push_back(Json::array({"summarization",missionId,missionId,snapshot->forkContextMessages}));
  return std::function<void()>([&]{trace.push_back(Json::array({"stopSummary"}));});};
 imports.isLocalBatMission=[](const Json& mission){return field(mission,"type")=="local_agent";};
 imports.updateProgressFromMessage=[&](Json& tracker,const Json& message,
    const ba::ActivityResolver&,const batbox::gadgets::Gadgets& gadgets){
  Json names=Json::array();for(auto* gadget:gadgets)names.push_back(gadget->name);
  trace.push_back(Json::array({"updateProgress",field(message,"type"),names}));
  tracker["tokenCount"]=field(tracker,"tokenCount").get<int>()+1;
  tracker["toolUseCount"]=field(tracker,"toolUseCount").get<int>()+
    (field(message,"type")=="assistant"?1:0);};
 imports.getProgressUpdate=[&](const Json& tracker){
  trace.push_back(Json::array({"getProgress"}));return tracker;};
 imports.updateAsyncBatProgress=[&](const std::string& id,const Json& progress,
    const ba::RootSetAppState&){trace.push_back(Json::array({"taskProgress",id,progress}));};
 imports.emitMissionProgressEvent=[&](const Json& event){
  trace.push_back(Json::array({"emitProgress",event}));};
 imports.getTokenCountFromTracker=[&](const Json& tracker){
  trace.push_back(Json::array({"trackerTokens"}));
  return field(tracker,"tokenCount").get<double>();};
 imports.result.getLastAssistantMessage=[&](const Json& all){
  trace.push_back(Json::array({"lastAssistant"}));
  if(all.is_array())for(auto it=all.rbegin();it!=all.rend();++it)
   if(field(*it,"type")=="assistant")return *it;
  return Json(nullptr);};
 imports.result.getTokenCountFromUsage=[&](const Json& usage){
  trace.push_back(Json::array({"tokens"}));
  const Json& in=field(usage,"input_tokens");const Json& out=field(usage,"output_tokens");
  return (in.is_number()?in.get<double>():0.0)+(out.is_number()?out.get<double>():0.0);};
 imports.result.extractTextContent=[&](const Json& blocks,const std::string& separator){
  trace.push_back(Json::array({"extract"}));std::string result;
  if(blocks.is_array())for(const Json& block:blocks)if(field(block,"type")=="text"){
   if(!result.empty())result+=separator;
   const Json& text=field(block,"text");if(text.is_string())result+=text.get<std::string>();}
  return result;};
 imports.result.nowMillis=[](){return std::int64_t(1000);};
 const auto syncRetained=[&](const ba::BatMessageRefs& refs){
  if(!flag(item,"retain"))return;
  Json messages=Json::array();for(const auto& ref:refs)messages.push_back(*ref);
  state["tasks"]["task"]["messages"]=std::move(messages);
 };
 imports.completeAsyncBat=[&](const Json& result,const ba::BatMessageRefs& refs,
   const ba::RootSetAppState&){
  syncRetained(refs);
  trace.push_back(Json::array({"complete",result}));state["tasks"]["task"]["status"]="completed";};
 imports.feature=[&](const std::string& name){trace.push_back(Json::array({"feature",name}));
  return flag(item,"classifier");};
 imports.classifyHandoffIfNeeded=[&](const Json&,const batbox::gadgets::Gadgets&,
    const Json&,std::stop_token,const std::string& type,int){
  trace.push_back(Json::array({"classify",type}));
  const Json& warning=field(item,"warning");
  return warning.is_string()?std::optional<std::string>(warning.get<std::string>()):
   std::nullopt;};
 imports.enqueueBatNotification=[&](const Json& notice,const ba::RootSetAppState&){
  trace.push_back(Json::array({"notify",notice}));};
 imports.killAsyncBat=[&](const std::string& id,const ba::BatMessageRefs& refs,
   const ba::RootSetAppState&){
  syncRetained(refs);
  trace.push_back(Json::array({"kill",id}));state["tasks"]["task"]["status"]="killed";};
 imports.failAsyncBat=[&](const std::string& id,const std::string& message,
    const ba::BatMessageRefs& refs,const ba::RootSetAppState&){syncRetained(refs);
   trace.push_back(Json::array({"fail",id,message}));
   state["tasks"]["task"]["status"]="failed";};
 imports.errorMessage=[&](std::exception_ptr error){trace.push_back(Json::array({"errorMessage"}));
  try{std::rethrow_exception(error);}catch(const std::exception& exception){
   return std::string(exception.what());}};
 imports.clearInvokedSkillsForBat=[&](const std::string& id){
  trace.push_back(Json::array({"clearSkills",id}));};
 Json error=nullptr;
 try{ba::runAsyncBatLifecycle(params,imports);}
 catch(const std::exception& exception){error=exception.what();}
 return Json{{"trace",trace},{"state",state},{"error",error}};
}
int main(){std::string line;while(std::getline(std::cin,line))try{
 std::cout<<value::stringify(run(value::parse(line)))<<'\n';
 }catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}}
