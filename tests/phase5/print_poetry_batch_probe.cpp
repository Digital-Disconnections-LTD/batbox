#include "cli/headless/PoetryBatch.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <iostream>
#include <optional>
#include <string>

using namespace batbox;
namespace {
std::optional<std::string> stringField(const Json& value,const char* key){
  const auto& found=value::property(value,key);
  return found.is_string()?std::optional(found.get<std::string>()):std::nullopt;
}
std::optional<bool> boolField(const Json& value,const char* key){
  const auto& found=value::property(value,key);
  return found.is_boolean()?std::optional(found.get<bool>()):std::nullopt;
}
cli::headless::QueuedCommandBatchKey command(const Json& value){
  return {value.at("mode").get<std::string>(),
    stringField(value,"workload"),boolField(value,"isMeta")};
}
Json run(const Json& input,cli::headless::ReceivedUuidTracker& tracker){
  const auto kind=input.at("kind").get<std::string>();
  if(kind=="join"){
    std::vector<cli::headless::PoetryValue> values;
    for(const auto& raw:input.at("values")){
      if(raw.is_string())values.emplace_back(raw.get<std::string>());
      else values.emplace_back(
        std::make_shared<std::vector<Json>>(raw.get<std::vector<Json>>()));
    }
    const auto result=cli::headless::joinPoetryValues(values);
    Json output;
    std::optional<bool> sameArray;
    if(const auto text=std::get_if<std::string>(&result))output=*text;
    else{
      output=*std::get<cli::headless::ContentBlocks>(result);
      if(values.size()==1&&std::holds_alternative<
          cli::headless::ContentBlocks>(values[0]))
        sameArray=std::get<cli::headless::ContentBlocks>(result)==
          std::get<cli::headless::ContentBlocks>(values[0]);
    }
    return Json{{"value",output},
      {"sameArray",sameArray?Json(*sameArray):Json()}};
  }
  if(kind=="batch"){
    const auto& next=value::property(input,"next");
    return Json{{"value",cli::headless::canBatchWith(
      command(input.at("head")),next.is_object()?std::optional(command(next)):
        std::nullopt)}};
  }
  if(kind=="track"){
    std::vector<bool> accepted;
    for(const auto& value:input.at("ids"))
      accepted.push_back(tracker.track(value.get<std::string>()));
    Json first=Json::array(),last=Json::array();
    for(std::size_t i=0;i<accepted.size()&&i<3;++i)
      first.push_back(bool(accepted[i]));
    for(std::size_t i=accepted.size()>3?accepted.size()-3:0;
        i<accepted.size();++i)last.push_back(bool(accepted[i]));
    const auto count=std::count(accepted.begin(),accepted.end(),true);
    return Json{{"accepted",count},{"first",first},{"last",last},
      {"cap",cli::headless::maxReceivedUuids}};
  }
  throw std::invalid_argument("Unknown input kind");
}
}
int main(){
  cli::headless::ReceivedUuidTracker tracker;
  std::string line;
  while(std::getline(std::cin,line))
    std::cout<<value::stringify(run(value::parse(line),tracker))<<'\n';
}
