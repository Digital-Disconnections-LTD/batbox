#include "instructions/hooks/runtime/FlightEnvironment.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
struct FsError:std::runtime_error {
  std::string code;
  FsError(std::string value,std::string message):
    std::runtime_error(std::move(message)),code(std::move(value)){}
};
Json run(const Json& item){
  event_loop::Loop loop;Json calls=Json::array();Json files=item.at("files");
  hook::FlightEnvironmentImports imports;
  imports.getConfigHomeDir=[&]{calls.push_back("home");return "/config";};
  imports.getFlightId=[&]{calls.push_back("session");return "s1";};
  imports.getPlatform=[&]{calls.push_back("platform");
    return item.value("platform",std::string("linux"));};
  imports.getParentEnvFile=[&]()->std::optional<std::string>{
    if(item.contains("parent"))return item.at("parent").get<std::string>();
    return std::nullopt;};
  imports.mkdir=[&](const std::string& path){
    calls.push_back("mkdir:"+path);
    event_loop::Deferred<event_loop::Unit> value(loop);value.resolve({});return value;};
  imports.readdir=[&](const std::string& path){
    calls.push_back("readdir:"+path);
    event_loop::Deferred<std::vector<std::string>> value(loop);
    if(item.contains("readdirError")){
      const auto code=item.at("readdirError").get<std::string>();
      value.reject(std::make_exception_ptr(FsError(code,code)));
    }else value.resolve(item.at("listing").get<std::vector<std::string>>());
    return value;};
  imports.readFile=[&](const std::string& path){
    calls.push_back("read:"+path);
    event_loop::Deferred<std::string> value(loop);
    if(!files.contains(path))
      value.reject(std::make_exception_ptr(FsError("ENOENT","ENOENT")));
    else if(files.at(path).is_object()){
      const auto& failure=files.at(path);
      value.reject(std::make_exception_ptr(FsError(
        failure.at("error").get<std::string>(),
        failure.at("message").get<std::string>())));
    }else value.resolve(files.at(path).get<std::string>());
    return value;};
  imports.writeFile=[&](const std::string& path,const std::string& content){
    calls.push_back("write:"+path+":"+content);files[path]=content;
    event_loop::Deferred<event_loop::Unit> value(loop);value.resolve({});return value;};
  imports.logDebug=[&](const std::string& message){calls.push_back("log:"+message);};
  imports.errnoCode=[](std::exception_ptr error)->std::optional<std::string>{
    try{std::rethrow_exception(error);}catch(const FsError& value){return value.code;}
    catch(...){return std::nullopt;}
  };
  imports.errorMessage=[](std::exception_ptr error){
    try{std::rethrow_exception(error);}catch(const std::exception& value){
      return std::string(value.what());}
  };
  hook::FlightEnvironment flight(loop,std::move(imports));
  Json results=Json::array();
  for(const auto& actionValue:item.at("actions")){
    const auto action=actionValue.get<std::string>();
    Json result={{"action",action}};
    if(action=="invalidate"){
      flight.invalidateFlightEnvStash();result["value"]=nullptr;
    }else if(action=="load"){
      flight.getFlightEnvironmentScript().then(
        [&](const std::optional<std::string>* value,const std::exception_ptr& error){
        if(error){try{std::rethrow_exception(error);}catch(const std::exception& e){
          result["error"]=e.what();}return;}
        result["value"]=*value?Json(**value):Json(nullptr);
      });
      loop.run();
    }else if(action=="path"){
      flight.getHookEnvFilePath(item.at("event").get<std::string>(),
        item.at("index").get<std::size_t>()).then(
        [&](const std::string* value,const std::exception_ptr& error){
          if(error){try{std::rethrow_exception(error);}catch(const std::exception& e){
            result["error"]=e.what();}return;}
          result["value"]=*value;
        });
      loop.run();
    }else{
      flight.clearCwdEnvFiles().then([&](const event_loop::Unit*,
        const std::exception_ptr& error){
        if(error){try{std::rethrow_exception(error);}catch(const std::exception& e){
          result["error"]=e.what();}return;}
        result["value"]=nullptr;
      });
      loop.run();
    }
    results.push_back(std::move(result));
  }
  return {{"name",item.at("name")},{"results",results},{"calls",calls}};
}
}
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream input(argv[1]);Json cases;input>>cases;
  Json rows=Json::array();for(const auto& item:cases)rows.push_back(run(item));
  std::cout<<value::stringify(rows)<<'\n';
}
