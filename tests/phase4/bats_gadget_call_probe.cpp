#define main batGadgetCallFixtureMain
#include "bats_gadget_foreground_indigenous.cpp"
#undef main
#include "bats/BatGadgetCall.hpp"
#include <string>

int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    const Json item=batbox::value::parse(line);
    Fixture fixture;
    fixture.time=item.at("nowMillis").get<std::int64_t>();
    fixture.state["toolPermissionContext"]={{"mode","default"},
      {"additionalWorkingDirectories",Json::array()}};
    fixture.state["mcp"]={{"clients",Json::array()},{"tools",Json::array()}};
    Json bat={{"agentType",item.value("agentType",std::string("Explore"))},
      {"model","model"},{"source","built-in"}};
    fixture.state["agentDefinitions"]={{"activeAgents",Json::array({bat})}};
    fixture.context.query.data["options"]={{"agentDefinitions",
      {{"activeAgents",Json::array({bat})}}},{"tools",Json::array()},
      {"mainLoopModel","model"},{"mcpClients",Json::array()},
      {"querySource","main"}};
    fixture.context.query.data["messages"]=Json::array();
    if(item.value("fork",false))fixture.context.query.data["renderedSystemPrompt"]=
      Json::array({"parent prompt"});
    Json trace=Json::array();
    fixture.context.setGadgetJSX=[&](const Json& value){
      trace.push_back(Json::array({"jsx",value}));
    };
    fixture.services.isBackgroundMissionsDisabled=[&](){
      return !(item.value("runInBackground",false) ||
        item.value("backgroundTransition",false));
    };
    fixture.services.progress.onProgress=[&](const Json& progress){
      trace.push_back(Json::array({"progress",progress}));
      const auto& data=field(progress,"data");
      if(item.value("backgroundTransition",false) &&
          field(data,"type")=="agent_progress" && field(data,"prompt")=="")
        batbox::bats::backgroundBatMission(
          item.value("agentId",std::string("agent-1")),fixture.foreground);
    };
    fixture.services.progress.getAssistantMessageContentLength=[](const Json& message){
      double length=0;
      for(const auto& block:field(field(message,"message"),"content"))
        if(field(block,"type")=="text")
          length+=field(block,"text").get<std::string>().size();
      return length;
    };
    fixture.services.runBat=[item](const batbox::bats::RunBatParams& request,
        const batbox::query::Yield& yield){
      std::vector<batbox::query::MessageRef> refs;
      for(const auto& message:item.at("messages")){
        auto ref=std::make_shared<Json>(message);refs.push_back(ref);yield(ref);
        if(item.value("backgroundTransition",false)){
          while(!request.overrideAbortOwner->stop_requested())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
          return;
        }
      }
      if(item.contains("lateUsage"))for(auto it=refs.rbegin();it!=refs.rend();++it){
        if(field(**it,"type")!="assistant")continue;
        batbox::runtime::MessageLock lock(*it);
        lock.value()["message"]["usage"]["output_tokens"]=item.at("lateUsage");
        break;
      }
      if(item.value("abortAfter",false))throw batbox::bats::BatAbortError("aborted");
      if(item.value("throwAfter",false))throw std::runtime_error("runner failure");
    };
    batbox::bats::BatGadgetCallServices services;
    auto& preflight=services.preflight;
    preflight.nowMillis=[&](){return fixture.time;};
    preflight.sleepMillis=[](std::int64_t){};
    preflight.getAppState=[&](){return fixture.state;};
    preflight.isCoordinatorMode=[](){return false;};
    preflight.isBatSwarmsEnabled=[&](){return item.value("teamSpawn",false);};
    preflight.isPartner=[](){return false;};
    preflight.isInProcessPartner=[](){return false;};
    preflight.isForkSubagentEnabled=[&](){return item.value("fork",false);};
    preflight.isInForkChild=[](const Json&){return false;};
    preflight.filterDeniedBats=[](const Json& bats,const Json&,
      const std::string&){return bats;};
    preflight.getDenyRuleForBat=[](const Json&,const std::string&,
      const std::string&){return Json();};
    preflight.setBatColor=[](const std::string&,const Json&){};
    preflight.spawnPartner=[](const Json&){return Json{{"data",{{"agentId","helper-agent"}}}};};
    preflight.forkBat={{"agentType","Fork"}};
    preflight.generalPurposeBat={{"agentType","Explore"}};
    auto& launch=services.launch;
    launch.poetry.additionalWorkingDirectoryKeys=[](const Json&){return Json::array();};
    launch.poetry.getSelectedBatPoetry=[](const Json&,const batbox::bats::BatContext&){
      return Json::array({"agent prompt"});
    };
    launch.poetry.enhanceSystemPoetryWithEnvDetails=[](const Json&,
      const std::string&,const Json&){return Json::array({"enhanced"});};
    launch.poetry.createUserMessage=[](const Json& input){
      return Json{{"type","user"},{"message",{{"content",Json::array({
        Json{{"type","text"},{"text",field(input,"content")}}})}}}};
    };
    launch.poetry.buildForkedMessages=[](const std::string& poetry,const Json&){
      Json message={{"type","user"},{"message",{{"content",Json::array({
        Json{{"type","text"},{"text",poetry}}})}}}};
      return Json::array({message});
    };
    launch.getBatModel=[&](const Json&,const Json&,const Json&,const Json&){
      return item.value("resolvedModel",std::string("model"));
    };
    launch.isBuiltInBat=[](const Json&){return true;};
    launch.feature=[](const std::string&){return false;};
    launch.isForkSubagentEnabled=[&](){return item.value("fork",false);};
    launch.isBackgroundMissionsDisabled=[&](){
      return !(item.value("runInBackground",false) ||
        item.value("backgroundTransition",false));
    };
    launch.assembleGadgetPool=[](const Json&,const Json&){return batbox::gadgets::Gadgets{};};
    launch.createBatId=[&](){return item.value("agentId",std::string("agent-1"));};
    launch.createBatWorktree=[](const std::string&){return Json{
      {"worktreePath","/tmp/tree"},{"worktreeBranch","branch"},
      {"headCommit","head"},{"gitRoot","/repo"}};};
    launch.getQuerySourceForBat=[](const std::string&,bool){
      return std::string("agent:builtin:Explore");
    };
    launch.asSystemPoetry=[](const Json& value){return value;};
    services.async.registration=&fixture.registration;
    services.async.getParentFlightId=[]()->std::optional<std::string>{return std::nullopt;};
    services.async.isBuiltInBat=[](const Json&){return true;};
    services.async.isForkSubagentEnabled=[](){return false;};
    services.async.getSdkBatProgressSummariesEnabled=[](){return false;};
    services.async.runWithBatContext=[](const Json&,
      const std::function<void()>& fn){fn();};
    services.async.launchLifecycle=[](batbox::bats::AsyncLifecycleParams){};
    services.async.runBat=fixture.services.runBat;
    services.async.getMissionOutputPath=[](const std::string& id){return "/out/"+id;};
    services.foreground=fixture.services;
    services.foreground.launchBackgroundContinuation=[&](
        batbox::bats::BatGadgetBackgroundContinuation next){
      assert(next.missionId==item.value("agentId",std::string("agent-1")));
      assert(next.foregroundStream);
      next.foregroundStream->returnWithin(std::chrono::milliseconds(1000));
    };
    services.foreground.final.worktree.hasWorktreeChanges=[&](const std::string&,
      const std::string&){return item.value("worktreeChanged",false);};
    services.foreground.final.worktree.removeBatWorktree=[](const std::string&,
      const Json&,const Json&){};
    services.foreground.final.worktree.enqueueBatMetadata=[](const std::string&,
      const Json&){};
    Json input={{"prompt","Do work"},{"description","Explore"}};
    if(!item.value("fork",false))input["subagent_type"]=item.value("agentType",std::string("Explore"));
    if(item.value("runInBackground",false))input["run_in_background"]=true;
    if(item.value("teamSpawn",false)){
      input["team_name"]="team";input["name"]="helper";
    }
    if(item.value("worktree",false))input["isolation"]="worktree";
    try{
      const Json result=batbox::bats::callBatGadget(input,fixture.context,
        [](const Json&){std::promise<Json> promise;promise.set_value(Json());
          return promise.get_future();},Json{{"message",{{"id","m1"}}}},services);
      std::cout<<batbox::value::stringify(Json{{"result",result},
        {"responseLength",fixture.responseLength},{"trace",trace}})<<'\n';
    }catch(const std::exception& error){
      std::cout<<batbox::value::stringify(Json{{"error",error.what()},
        {"responseLength",fixture.responseLength},{"trace",trace}})<<'\n';
    }
  }
}
