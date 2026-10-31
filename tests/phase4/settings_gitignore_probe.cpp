#include "git/GitignoreUpdate.hpp"
#include "runtime/JsonValue.hpp"
#include <cerrno>
#include <iostream>
#include <map>
#include <system_error>

int main(){
  using namespace batbox;
  using namespace batbox::git;
  using event_loop::Deferred;
  using event_loop::Unit;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    event_loop::Loop loop;
    Json trace=Json::array(),logs=Json::array();
    Json files=test.value("files",Json::object());
    auto errorCode=[&](const std::string& operation)->std::optional<int>{
      if(!test.contains("errors")||!test["errors"].contains(operation))
        return std::nullopt;
      const auto name=test["errors"][operation].get<std::string>();
      if(name=="ENOENT")return ENOENT;
      if(name=="EACCES")return EACCES;
      return EIO;
    };
    auto reject=[&](const std::string& operation){
      return std::make_exception_ptr(std::system_error(
        errorCode(operation).value(),std::generic_category(),operation));
    };
    GitignoreServices services;
    services.homedir=[&](){trace.push_back({"home"});
      if(errorCode("home"))throw std::system_error(*errorCode("home"),
        std::generic_category(),"home");
      return test["home"].get<std::string>();};
    services.getCwd=[&](){trace.push_back({"cwd"});
      if(errorCode("cwd"))throw std::system_error(*errorCode("cwd"),
        std::generic_category(),"cwd");
      return test["defaultCwd"].get<std::string>();};
    services.dirIsInGitRepo=[&](const std::string& cwd){
      trace.push_back({"repo",cwd});Deferred<bool> result(loop);
      if(errorCode("repo"))result.reject(reject("repo"));
      else result.resolve(test.value("inRepo",false));
      return result;
    };
    services.execFileNoThrowWithCwd=[&](const std::string& file,
        const std::vector<std::string>& args,
        const process::ExecOptions& options){
      trace.push_back({"exec",file,args,
        options.preserveOutputOnError.value_or(true),
        options.cwd.value_or("")});
      Deferred<process::ExecResult> result(loop);
      if(errorCode("exec"))result.reject(reject("exec"));
      else result.resolve({"","",test.value("checkCode",1),{}});
      return result;
    };
    services.io.mkdirRecursive=[&](const std::string& path){
      trace.push_back({"mkdir",path,true});Deferred<Unit> result(loop);
      if(errorCode("mkdir"))result.reject(reject("mkdir"));
      else result.resolve(Unit{});
      return result;
    };
    services.io.readUtf8=[&](const std::string& path){
      trace.push_back({"read",path,"utf-8"});
      Deferred<std::string> result(loop);
      if(errorCode("read"))result.reject(reject("read"));
      else if(!files.contains(path))result.reject(std::make_exception_ptr(
        std::system_error(ENOENT,std::generic_category(),"read")));
      else result.resolve(files[path].get<std::string>());
      return result;
    };
    services.io.appendUtf8=[&](const std::string& path,
        const std::string& content){
      trace.push_back({"append",path,content});Deferred<Unit> result(loop);
      if(errorCode("append"))result.reject(reject("append"));
      else{files[path]=files.value(path,std::string{})+content;
        result.resolve(Unit{});}return result;
    };
    services.io.writeUtf8=[&](const std::string& path,
        const std::string& content){
      trace.push_back({"write",path,content,"utf-8"});Deferred<Unit> result(loop);
      if(errorCode("write"))result.reject(reject("write"));
      else{files[path]=content;result.resolve(Unit{});}return result;
    };
    services.logError=[&](std::exception_ptr error){
      try{std::rethrow_exception(error);}
      catch(const std::system_error& system){
        const auto code=system.code().value();
        logs.push_back(code==ENOENT?"ENOENT":code==EACCES?
          "EACCES":"EIO");
      }catch(const std::exception& exception){logs.push_back(exception.what());}
      if(errorCode("log"))throw std::system_error(*errorCode("log"),
        std::generic_category(),"log");
    };
    Json value=nullptr,rejected=nullptr;
    auto markRejection=[&](const std::exception_ptr& error){
      try{std::rethrow_exception(error);}
      catch(const std::system_error& system){
        const auto code=system.code().value();
        rejected=code==ENOENT?"ENOENT":code==EACCES?"EACCES":"EIO";
      }catch(const std::exception& exception){rejected=exception.what();}
    };
    const auto op=test["op"].get<std::string>();
    if(op=="path")value=getGlobalGitignorePath(services);
    else if(op=="ignored"){
      const auto result=isPathGitignored(loop,test["filename"],test["cwd"],
        services);
      bool done=false;
      result.then([&](const bool* ignored,const std::exception_ptr& error){
        if(error)markRejection(error);
        else value=*ignored;
        done=true;
      });
      for(int i=0;i<100&&!done;i++)loop.runReady();
    }else{
      const auto result=addFileGlobRuleToGitignore(loop,test["filename"],
        test.value("useDefaultCwd",false)?std::nullopt:
          std::optional<std::string>(test["cwd"]),services);
      bool done=false;
      result.then([&](const Unit*,const std::exception_ptr& error){
        if(error)markRejection(error);
        done=true;
      });
      for(int i=0;i<100&&!done;i++)loop.runReady();
    }
    std::cout<<value::stringify({{"value",value},{"rejected",rejected},
      {"trace",trace},
      {"logs",logs},{"files",files}})<<'\n';
  }
}
