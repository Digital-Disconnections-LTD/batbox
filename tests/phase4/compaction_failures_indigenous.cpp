#include "compaction/Poetry.hpp"
#include "compaction/Summary.hpp"
#include "streaming/Reducer.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
using namespace batbox::compaction;
namespace {
void require(bool condition,const std::string& message){
  if(!condition)throw std::runtime_error(message);
}
Json assistant(const std::string& text,bool apiError=false){
  Json message={{"type","assistant"},{"message",{{"role","assistant"},
    {"content",Json::array({Json{{"type","text"},{"text",text}}})}}}};
  if(apiError)message["isApiErrorMessage"]=true;
  return message;
}
struct Run {
  Context context;
  SummaryServices services;
  int forks=0,requests=0,sleeps=0;
  std::vector<std::string> logged;
  Json forkMessages=Json::array();
  bool forkThrows=false;
  std::string forkError="fork failed";
  Json fallbackReply=assistant("Fallback summary.");
  Json fallbackOptions;
  std::stop_source stop;
  Run(){
    context.data={{"options",{{"tools",Json::array()},{"mainLoopModel","local-model"},
      {"isNonInteractiveSession",true},{"agentDefinitions",{{"activeAgents",Json::array()}}}}}};
    context.signal=stop.get_token();
    context.getAppState=[]{return Json{{"toolPermissionContext",{{"mode","default"}}}};};
    services.poetryStashSharingEnabled=[]{return true;};
    services.streamingRetryEnabled=[]{return true;};
    services.runForkedBat=[this](const Json&,std::stop_token,const std::function<Json()>&,
        const std::shared_ptr<bats::StashSafeParams>&){
      ++forks;
      if(forkThrows)throw std::runtime_error(forkError);
      return Json{{"messages",forkMessages}};
    };
    services.isGadgetSearchEnabled=[](const Json&,const Json&,const std::function<Json()>&,
      const Json&,const std::string&){return false;};
    services.fileReadGadget={{"name","Read"}};
    services.normalizeMessagesForAPI=[](const Json& messages,const Json&){return messages;};
    services.getMaxOutputTokensForModel=[](const std::string&){return 50000.0;};
    services.queryModelWithStreaming=[this](const SummaryModelRequest& request)->model::NextMessage{
      ++requests;fallbackOptions=request.options;
      auto replies=std::make_shared<Json>(fallbackReply.is_array()?fallbackReply:Json::array({fallbackReply}));
      return [replies,index=std::size_t{0}]()mutable->std::optional<model::MessageRef>{
        if(index>=replies->size())return std::nullopt;
        return std::make_shared<Json>((*replies)[index++]);};
    };
    services.getRetryDelay=[](double){return 1.0;};
    services.sleep=[this](double,std::stop_token){++sleeps;};
    services.log=[this](const std::string& line){logged.push_back(line);};
  }
  Json summarize(){
    SummaryInputs input{Json::array({Json{{"type","user"},{"message",{{"content","work"}}}}}),
      Json{{"type","user"},{"message",{{"content","summarize"}}}},
      Json{{"toolPermissionContext",{{"mode","default"}}}},Json::object(),100};
    return streamCompactSummary(input,context,services);
  }
};
template<class Error,class Callback>bool throwsAs(Callback callback){
  try{callback();}catch(const Error&){return true;}catch(...){return false;}
  return false;
}
AutoServices autoServices(std::function<Json()> compact,std::vector<std::pair<std::string,bool>>& reports){
  AutoServices services;
  services.isAutoCompactEnabled=[]{return true;};
  services.tokenCountWithEstimation=[](const Json&){return 200000.0;};
  services.calculateTokenWarningState=[](double,const std::string&){
    return Json{{"isAboveAutoCompactThreshold",true}};};
  services.getAutoCompactThreshold=[](const std::string&){return 100000.0;};
  services.runPostCompactCleanup=[](const std::optional<std::string>&){};
  services.compactConversation=[compact](const Json&,const Json&,const Json&,bool,
      const std::optional<std::string>&,bool,const Json&){return compact();};
  services.reportFailure=[&reports](const std::string& reason,bool timedOut){
    reports.emplace_back(reason,timedOut);};
  return services;
}
Json automatic(AutoServices& services,const Json& tracking){
  const Json context={{"options",{{"mainLoopModel","local-model"}}}};
  return autoCompactIfNeeded(Json::array({Json{{"type","user"}}}),context,Json::object(),
    std::string("repl_main_thread"),tracking,services);
}
}
int main(){
  {
    Run run;run.forkMessages=Json::array({assistant("Shared-prefix summary.")});
    require(messages::getAssistantMessageText(run.summarize())==std::string("Shared-prefix summary.")&&
      run.requests==0,"A working fork still sent a fallback request");
  }
  {
    Run run;run.forkMessages=Json::array({assistant("API Error: Request timed out",true)});
    require(throwsAs<CompactionTimeout>([&]{run.summarize();})&&run.forks==1&&run.requests==0,
      "A timed-out fork resent the whole conversation");
  }
  {
    Run run;run.forkThrows=true;run.forkError="Stream idle timeout - no chunks received";
    require(throwsAs<CompactionTimeout>([&]{run.summarize();})&&run.requests==0,
      "A fork that threw a timeout resent the whole conversation");
  }
  {
    Run run;run.forkMessages=Json::array({assistant("Prompt is too long: 300000 tokens",true)});
    const auto result=run.summarize();
    require(messages::getAssistantMessageText(result)->starts_with("Prompt is too long")&&
      run.requests==0,"A fork that was too long was retried at full length");
  }
  {
    Run run;run.forkThrows=true;run.stop.request_stop();
    require(throwsAs<std::runtime_error>([&]{run.summarize();})&&run.requests==0,
      "A cancelled compaction fell back to another request");
  }
  {
    Run run;run.forkMessages=Json::array({assistant("API Error: 400 bad request shape",true)});
    run.services.summaryAttempts=[]{return 1;};
    run.fallbackReply=Json{{"type","system"}};
    require(throwsAs<std::runtime_error>([&]{run.summarize();})&&run.requests==1&&run.sleeps==0,
      "One summary attempt sent more than one request");
    require(run.logged.size()==1&&run.logged[0].find("400 bad request shape")!=std::string::npos,
      "The fork failure was not logged with its reason");
  }
  {
    // A summary that reaches the output cap keeps the text produced so far.
    auto capped=assistant(streaming::maxOutputTokensMessage(50000,false),true);
    for(const auto* part:{"50000-token output cap","max_output_tokens","/config","BATBOX_MAX_OUTPUT_TOKENS"})
      require(streaming::maxOutputTokensMessage(50000,false).find(part)!=std::string::npos,
        std::string("The output cap error does not mention ")+part);
    capped["apiError"]="max_output_tokens";
    Run run;run.forkMessages=Json::array({assistant("<summary>Part one, "),
      assistant("part two"),capped});
    auto result=run.summarize();
    require(messages::getAssistantMessageText(result)==std::string("<summary>Part one, part two")&&
      result.value("compactSummaryCut",false)&&run.requests==0,"A fork summary cut at the cap was not kept");
    Run fallback;fallback.services.poetryStashSharingEnabled=[]{return false;};
    fallback.fallbackReply=Json::array({assistant("<summary>Cut here"),capped});
    result=fallback.summarize();
    require(result.value("compactSummaryCut",false)&&
      fallback.fallbackOptions.at("maxOutputTokensOverride")==50000,
      "The fallback summary was not kept or asked for a different cap than the fork");
    const auto note=getCompactUserSummaryMessage("<summary>Cut here",false,"/tmp/t.jsonl",50000);
    require(note.find("Summary:\nCut here")!=std::string::npos&&
      note.find("50000-token output limit")!=std::string::npos&&note.find("/tmp/t.jsonl")!=std::string::npos,
      "The summary message did not say it was cut and where the transcript is");
    require(getCompactPoetry(std::nullopt,16384).find("under about 12200 tokens")!=std::string::npos,
      "The summary prompt did not state a length inside its cap");
  }
  {
    Run run;run.forkThrows=true;
    run.fallbackReply=Json{{"type","system"}};
    require(throwsAs<std::runtime_error>([&]{run.summarize();})&&run.requests==2&&run.sleeps==1,
      "Remote endpoints lost the second summary attempt");
  }

  std::vector<std::pair<std::string,bool>> reports;
  auto timeout=autoServices([]()->Json{throw CompactionTimeout("The summary request timed out");},reports);
  auto result=automatic(timeout,Json{{"consecutiveFailures",0}});
  require(result.at("wasCompacted")==false&&result.at("consecutiveFailures")==3&&
    reports.size()==1&&reports[0].second,"A timeout was retried in the same submission or not reported");
  require(automatic(timeout,Json{{"consecutiveFailures",3}}).size()==1&&reports.size()==1,
    "Compaction ran past its failure limit");
  reports.clear();
  auto failing=autoServices([]()->Json{throw std::runtime_error("API Error: 500 server error");},reports);
  failing.failureLimit=5;
  result=automatic(failing,Json{{"consecutiveFailures",3}});
  require(result.at("consecutiveFailures")==4&&reports.size()==1&&!reports[0].second&&
    reports[0].first=="API Error: 500 server error","An error was not counted and reported");
  require(automatic(failing,Json{{"consecutiveFailures",5}}).size()==1&&reports.size()==1,
    "autoCompactFailureLimit was not honoured");
  reports.clear();
  auto aborted=autoServices([]()->Json{throw model::UserAbortError();},reports);
  result=automatic(aborted,Json{{"consecutiveFailures",1}});
  require(!result.contains("consecutiveFailures")&&reports.empty(),
    "A cancelled compaction was counted as a failure");
  std::cout<<"Compaction stops after timeouts, keeps fallbacks for real errors and reports why\n";
}
