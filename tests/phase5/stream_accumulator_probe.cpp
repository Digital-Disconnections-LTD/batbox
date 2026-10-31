#include "cli/remote/StreamAccumulator.hpp"
#include <iostream>

using namespace batbox;
using namespace batbox::cli::remote;
namespace {
Json run(const Json& input){
  auto state=createStreamAccumulator();
  Json outputs=Json::array();
  for(const auto& action:input.at("actions")){
    if(action.at("op")=="accumulate")
      outputs.push_back(accumulateStreamEvents(
        action.at("buffer").get<std::vector<Json>>(),state));
    else clearStreamAccumulatorForMessage(state,action.at("assistant"));
  }
  Json byMessage=Json::object(),scopes=Json::object();
  for(const auto& [id,blocks]:state.byMessage){
    Json entries=Json::object();
    for(const auto& [index,chunks]:blocks)entries[index]=*chunks;
    byMessage[id]=std::move(entries);
  }
  for(const auto& [scope,id]:state.scopeToMessage)scopes[scope]=id;
  return Json{{"outputs",outputs},{"byMessage",byMessage},
    {"scopeToMessage",scopes}};
}
}
int main(){
  std::string line;
  while(std::getline(std::cin,line))try{
    std::cout<<value::stringify(run(value::parse(line)))<<'\n';
  }catch(const std::exception& error){
    std::cerr<<error.what()<<'\n';return 1;
  }
}
