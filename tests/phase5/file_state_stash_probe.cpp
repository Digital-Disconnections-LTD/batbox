#include "file_state_stash/FileStateStash.hpp"
#include <iostream>

using namespace batbox;
namespace {
using file_state_stash::FileStateStash;
Json snapshot(const FileStateStash& stash){
  Json keys=Json::array(),entries=Json::array();
  for(const auto& key:file_state_stash::stashKeys(stash))keys.push_back(key);
  for(const auto& [key,value]:stash.entries())
    entries.push_back(Json::array({key,file_state_stash::toJson(*value)}));
  return {{"size",stash.size()},{"max",stash.max()},
    {"maxSize",stash.maxSize()},{"calculatedSize",stash.calculatedSize()},
    {"keys",keys},{"entries",entries},{"dump",stash.dumpJson()},
    {"object",file_state_stash::stashToJson(stash)}};
}
Json run(const Json& input){
  FileStateStash stash(input.at("max").get<std::size_t>(),
    input.at("maxSize").get<std::size_t>());
  Json trace=Json::array();
  for(const auto& action:input.at("ops")){
    const auto op=action.at("op").get<std::string>();
    Json result=nullptr;
    if(op=="set"){
      result=&stash==&stash.set(action.at("key").get<std::string>(),
        file_state_stash::fromJson(action.at("value")));
    }else if(op=="get"){
      const auto value=stash.get(action.at("key").get<std::string>());
      if(value)result=file_state_stash::toJson(*value);
    }else if(op=="has")result=stash.has(action.at("key").get<std::string>());
    else if(op=="delete")result=stash.remove(action.at("key").get<std::string>());
    else if(op=="clear")stash.clear();
    else if(op=="mutate"){
      const auto value=stash.get(action.at("key").get<std::string>());
      value->content=action.at("content").get<std::string>();
      result=stash.get(action.at("key").get<std::string>())==value;
    }else if(op=="clone"){
      const auto copy=file_state_stash::cloneFileStateStash(stash);
      const auto key=action.at("key").get<std::string>();
      const auto original=stash.get(key),cloned=copy->get(key);
      result={{"sameValue",original==cloned},{"copy",snapshot(*copy)}};
    }else if(op=="merge"){
      FileStateStash second(stash.max(),stash.maxSize());
      for(const auto& entry:action.at("values"))
        second.set(entry.at("key").get<std::string>(),
          file_state_stash::fromJson(entry.at("value")));
      const auto merged=file_state_stash::mergeFileStateStashes(stash,second);
      result={{"merged",snapshot(*merged)}};
    }else if(op=="reload"){
      const auto key=action.at("key").get<std::string>();
      const auto prior=stash.get(key);
      const auto dump=stash.dump();stash.load(dump);
      result={{"sameValue",stash.get(key)==prior}};
    }else if(op=="load"){
      stash.loadJson(action.at("entries"));
    }else throw std::invalid_argument("Unknown operation "+op);
    trace.push_back({{"op",op},{"result",result},{"snapshot",snapshot(stash)}});
  }
  return trace;
}
}
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
    catch(const std::exception& error){
      std::cout<<value::stringify(Json{{"error",error.what()}})<<'\n';
    }
  }
}
