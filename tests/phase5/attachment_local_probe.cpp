#include "attachments/local/LocalCollectors.hpp"
#include "file_state_stash/FileStateStash.hpp"
#include <iostream>
#include <stdexcept>
#include <system_error>

using namespace batbox;
namespace {
template<class T>event_loop::Deferred<T> ready(event_loop::Loop& loop,T value){
  event_loop::Deferred<T> output(loop);output.resolve(std::move(value));return output;
}
Json await(event_loop::Loop& loop,event_loop::Deferred<Json> pending){
  std::optional<Json> value;std::exception_ptr error;
  pending.then([&](const Json* result,const std::exception_ptr& failure){
    if(result)value=*result;
    error=failure;
  });
  for(int i=0;i<50&&!value&&!error;++i)loop.runReady();
  if(error)std::rethrow_exception(error);
  if(!value)throw std::runtime_error("Local collector did not settle");
  return *value;
}
Json run(const Json& input){
  event_loop::Loop loop;
  attachments::local::Services services(loop);
  Json trace=Json::array();
  bootstrap::FlightTransitions transitions;
  transitions.hasExitedPlanMode=input.value("exited",false);
  transitions.needsPlanModeExitAttachment=input.value("needs",false);
  services.transitions=&transitions;
  auto app=std::make_shared<app_state::AppState>();
  app->fields=std::make_shared<value::SharedRecord>(
    value::SharedRecord::fromJson(Json{{"todos",Json{{"session",
      input.value("todos",Json::array())}}},
      {"pendingPlanVerification",input.value("pending",Json())}}));
  app->permissionContext=std::make_shared<value::SharedRecord>(
    value::SharedRecord::fromJson(Json{{"mode",
      input.value("mode",std::string("default"))}}));
  services.getAppState=[&]{trace.push_back("appState");return app;};
  services.planFilePath=[&](const std::optional<std::string>& id){
    trace.push_back("planPath");return "/plans/"+id.value_or("main");
  };
  services.getPlan=[&](const std::optional<std::string>&){
    trace.push_back("getPlan");return input.value("plan",false)?
      std::optional<Json>("plan"):std::nullopt;
  };
  services.flightId=[&]{trace.push_back("sessionId");return std::string("session");};
  services.missionListId=[&]{trace.push_back("taskListId");return std::string("list");};
  services.listMissions=[&](const std::string&){
    trace.push_back("listTasks");return ready(loop,input.value("tasks",Json::array()));
  };
  services.todoV2Enabled=[&]{return input.value("todoV2",true);};
  services.internalUser=[&]{return input.value("internal",false);};
  services.verifyPlanEnabled=[&]{return input.value("verify",false);};
  services.getNewDiagnostics=[&]{
    trace.push_back("newDiagnostics");
    return ready(loop,input.value("diagnostics",Json::array()));
  };
  services.checkLspDiagnostics=[&]{
    trace.push_back("checkLsp");return input.value("lsp",Json::array());
  };
  services.clearLspDiagnostics=[&]{trace.push_back("clearLsp");};
  services.generateMissionAttachments=[&](const app_state::AppStateRef&){
    trace.push_back("generateTasks");
    return ready(loop,attachments::local::MissionBatch{
      input.value("taskAttachments",Json::array()),Json{{"next",1}},
      Json::array({"old"})});
  };
  services.applyMissionOffsetsAndEvictions=[&](const Json&,const Json&){
    trace.push_back("applyTasks");
  };
  services.missionOutputPath=[&](const std::string& id){
    trace.push_back("outputPath");return "/tasks/"+id;
  };
  services.structuredPatch=[&](const std::string&,const std::string&,
      const std::string&,int){
    return input.value("patch",Json{{"hunks",Json::array()}});
  };
  services.compactLinePrefixEnabled=[](){return false;};
  services.logError=[&](std::exception_ptr){trace.push_back("error");};
  services.logDebug=[](const std::string&){};
  services.readImageWithTokenBudget=[&](const std::string&){
    trace.push_back("image");return ready(loop,Json{{"base64","YQ=="}});
  };
  services.files=std::make_shared<attachments::files::Services>(loop);
  services.files->expandPath=[&](const std::string& path){
    trace.push_back("expand");return path;
  };
  services.files->isFileReadDenied=[&](const std::string&,
      const app_state::AppStateRef&){
    trace.push_back("deny");return input.value("denied",false);
  };
  services.files->modificationTime=[&](const std::string&){
    trace.push_back("mtime");
    event_loop::Deferred<double> result(loop);
    if(input.value("fileError",std::string())=="ENOENT")
      result.reject(std::make_exception_ptr(std::system_error(
        std::make_error_code(std::errc::no_such_file_or_directory))));
    else result.resolve(input.value("mtime",0.0));
    return result;
  };
  services.files->validateRead=[&](const Json&,execution::Context&){
    trace.push_back("validate");
    gadgets::ValidationResult result;result.result=input.value("valid",true);
    return ready(loop,result);
  };
  services.files->callRead=[&](const Json&,execution::Context&){
    trace.push_back("read");
    gadgets::GadgetResult result;
    result.data=input.value("readData",Json{{"type","text"},
      {"file",Json{{"content","new"}}}});
    return ready(loop,result);
  };
  gadgets::Gadget todo,update,brief,bash;
  todo.name="TodoWrite";update.name="TaskUpdate";
  brief.name="Brief";bash.name="Bash";
  execution::Context context;
  const auto& names=value::property(input,"tools");
  if(names.is_array())for(const auto& name:names){
    if(name=="TodoWrite")context.gadgets.push_back(&todo);
    else if(name=="TaskUpdate")context.gadgets.push_back(&update);
    else if(name=="Brief")context.gadgets.push_back(&brief);
    else if(name=="Bash")context.gadgets.push_back(&bash);
  }
  context.data=Json::object();
  if(input.contains("agentId"))context.data["agentId"]=input.at("agentId");
  context.readFileState=file_state_stash::createFileStateStashWithSizeLimit(100);
  if(input.contains("fileState"))
    context.readFileState->set("/workspace/a.txt",
      file_state_stash::fromJson(input.at("fileState")));
  const auto messages=input.value("messages",Json::array());
  const auto op=input.at("op").get<std::string>();
  Json result;
  if(op=="plan")result=attachments::local::getPlanModeAttachments(
    messages,context,services);
  else if(op=="exit")result=attachments::local::getPlanModeExitAttachment(
    context,services);
  else if(op=="todo")result=attachments::local::getTodoReminderAttachments(
    messages,context,services);
  else if(op=="task")result=await(loop,
    attachments::local::getMissionReminderAttachments(messages,context,services));
  else if(op=="diag")result=await(loop,
    attachments::local::getDiagnosticAttachments(context,services));
  else if(op=="lsp")result=attachments::local::getLspDiagnosticAttachments(
    context,services);
  else if(op=="unified")result=await(loop,
    attachments::local::getUnifiedMissionAttachments(services));
  else if(op=="verify")result=attachments::local::getVerifyPlanReminderAttachment(
    messages,services);
  else if(op=="changed")result=await(loop,
    attachments::local::getChangedFiles(context,services));
  else if(op=="snippet")result=attachments::local::getSnippetForTwoFileDiff(
    input.value("before",std::string()),input.value("after",std::string()),
    services);
  else throw std::invalid_argument("Unknown local operation");
  return Json{{"result",result},{"trace",trace},
    {"exited",transitions.hasExitedPlanMode},
    {"needs",transitions.needsPlanModeExitAttachment},
    {"cachePresent",context.readFileState->has("/workspace/a.txt")}};
}
}
int main(){std::string line;while(std::getline(std::cin,line)){
  try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
  catch(const std::exception& error){
    std::cerr<<error.what()<<'\n';return 1;
  }
}}
