#include "query/MemoryAttachmentFilter.hpp"
#include <iostream>

using namespace batbox;
namespace {
Json run(const Json& input){
  file_state_stash::FileStateStash stash(input.value("max",100),
    input.value("maxSize",1024));
  if(input.contains("initial"))for(const Json& entry:input.at("initial"))
    stash.set(entry.at(0).get<std::string>(),
      file_state_stash::fromJson(entry.at(1)));
  Json trace=Json::array();
  for(const Json& attachments:input.at("rounds")){
    Json result=query::filterDuplicateMemoryAttachments(attachments,stash);
    Json entries=Json::array();
    for(const auto& [key,value]:stash.entries())
      entries.push_back(Json::array({key,file_state_stash::toJson(*value)}));
    trace.push_back(Json{{"result",std::move(result)},
      {"entries",std::move(entries)}});
  }
  return trace;
}
}
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
    catch(const std::exception& error)
      {std::cout<<value::stringify(Json{{"error",error.what()}})<<'\n';}
  }
}
