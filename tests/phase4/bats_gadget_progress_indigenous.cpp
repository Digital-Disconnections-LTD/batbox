#include "bats/BatGadgetPresentation.hpp"
#include <cassert>
#include <iostream>

using namespace batbox;
int main(){
  const Json event={{"timestamp","PRIVATE_TIMESTAMP"},{"data",{
    {"type","agent_progress"},{"agentId","PRIVATE_AGENT_ID"},
    {"message",{{"type","assistant"},{"message",{{"content",Json::array({
      {{"type","tool_use"},{"id","read-1"},{"name","Read"},
       {"input",{{"file_path","PRIVATE_PATH"}}}}
    })}}}}}
  }}};
  assert(bats::batGadgetProgressText(Json())=="Working…");
  assert(bats::batGadgetProgressText(Json::array({nullptr,Json::object()}))=="Working…");
  const auto progress=Json::array({event,event});
  assert(bats::batGadgetProgressText(progress)=="Latest tool: Read · 1 tool call");
  Json entries=Json::array();
  for(const auto* description:{"Active research","Queued research","Finished research","Failed research"})
    entries.push_back(Json{{"param",{{"input",{{"description",description},
      {"prompt","PRIVATE_PROMPT"}}}}},{"progressMessages",progress}});
  // Progress can arrive before the UI's in-progress IDs are published.
  entries[0]["isInProgress"]=false;
  entries[1]["progressMessages"]=Json::array();
  entries[2]["isResolved"]=true;
  entries[3]["isError"]=true;
  const auto text=bats::groupedBatGadgetUseText(entries);
  assert(text.find("4 Bats")!=std::string::npos);
  assert(text.find("Active research · Latest tool: Read · 1 tool call")!=std::string::npos);
  assert(text.find("Queued research · Queued")!=std::string::npos);
  assert(text.find("Finished research · Completed")!=std::string::npos);
  assert(text.find("Failed research · Failed")!=std::string::npos);
  assert(text.find("PRIVATE_")==std::string::npos);
  // A background launch resolves at once, so its card says Launched until
  // the Bat's own notification reports how it ended.
  Json launched=Json::array({Json{{"param",{{"input",{{"description","Background audit"}}}}},
    {"isResolved",true},{"output",{{"status","async_launched"},{"agentId","PRIVATE_AGENT_ID"}}}},
    Json{{"param",{{"input",{{"description","Foreground map"}}}}},{"isResolved",true},
    {"output",{{"status","completed"}}}}});
  const auto launchText=bats::groupedBatGadgetUseText(launched);
  if(launchText.find("Background audit · Launched")==std::string::npos||
      launchText.find("Foreground map · Completed")==std::string::npos||
      launchText.find("Background audit · Completed")!=std::string::npos){
    std::cerr<<"Background Bat launches must not read as completed:\n"<<launchText<<"\n";
    return 1;
  }
  // A sub-Bat's card names the Bat that launched it ahead of its status.
  const Json by=Json{{"data",{{"launchedBy","Audit the parser"}}}};
  Json nested=Json::array({
    Json{{"param",{{"input",{{"description","Check the lexer"}}}}},
      {"progressMessages",Json::array({by})}},
    Json{{"param",{{"input",{{"description","Count the tokens"}}}}},{"isInProgress",true},
      {"progressMessages",Json::array({event,by})}},
    Json{{"param",{{"input",{{"description","Split the grains"}}}}},{"isResolved",true},
      {"progressMessages",Json::array({by})},{"output",{{"status","completed"}}}}});
  const auto nestedText=bats::groupedBatGadgetUseText(nested);
  for(const auto* line:{"Check the lexer · Launched by Audit the parser\n",
      "Count the tokens · Launched by Audit the parser · Latest tool: Read · 1 tool call",
      "Split the grains · Launched by Audit the parser · Completed"})
    if(nestedText.find(line)==std::string::npos){
      std::cerr<<"Sub-Bat cards must name their launcher, missing '"<<line<<"':\n"<<nestedText<<"\n";
      return 1;
    }
  if(bats::batLaunchedBy(Json::array({event,by}))!="Audit the parser"||
      !bats::batLaunchedBy(progress).empty()){
    std::cerr<<"Launcher lookup changed\n";return 1;
  }
  std::cout<<"Bat progress displays activity and status without internal event data\n";
}
