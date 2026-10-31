#include "instructions/hooks/runtime/HookPoetryStream.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
Json run(const Json& item){
  event_loop::Loop loop;Json calls=Json::array();std::string stdoutText;
  hook::HookPoetryServices services;
  services.parseJson=[](const std::string& text){return value::parse(text);};
  services.requestPoetry=[&](const Json& request){
    calls.push_back("request:"+value::stringify(request));
    event_loop::Deferred<Json> response(loop);
    const auto id=request.at("prompt").get<std::string>();
    auto complete=[&,response,id]{
      if(item.value("reject",std::string{})==id)
        response.reject(std::make_exception_ptr(std::runtime_error("declined")));
      else response.resolve(item.at("responses").at(id));
    };
    if(item.value("delayFirst",false)&&id=="p1")loop.postMission(complete);
    else complete();
    return response;};
  services.writeStdin=[&](const std::string& text){calls.push_back("write:"+text);};
  services.destroyStdin=[&]{calls.push_back("destroy");};
  services.logDebug=[&](const std::string& text){calls.push_back("log:"+text);};
  services.stringifyThrown=[](std::exception_ptr error){
    try{std::rethrow_exception(error);}catch(const std::exception& value){
      return std::string("Error: ")+value.what();}
  };
  auto poetry=std::make_shared<hook::HookPoetryStream>(loop,std::move(services));
  for(const auto& chunk:item.at("chunks")){
    const auto data=chunk.get<std::string>();stdoutText+=data;
    poetry->onStdout(data);
  }
  bool finished=false;
  poetry->pending().then([&](const event_loop::Unit*,const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);finished=true;});
  loop.run();
  if(!finished)throw std::runtime_error("Hook prompt chain did not settle");
  return {{"name",item.at("name")},{"calls",calls},
    {"processed",poetry->processedCount()},
    {"finalStdout",poetry->removeProcessedLines(stdoutText)}};
}
}
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream input(argv[1]);Json cases;input>>cases;
  Json rows=Json::array();for(const auto& item:cases)rows.push_back(run(item));
  std::cout<<value::stringify(rows)<<'\n';
}
