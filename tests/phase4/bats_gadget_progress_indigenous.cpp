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
  std::cout<<"Bat progress displays activity and status without internal event data\n";
}
