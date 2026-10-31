#include "settings/mdm/RawRead.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

int main(){
  using namespace batbox;
  using namespace batbox::settings::mdm;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    event_loop::Loop loop;
    Json trace=Json::array(),value=nullptr,rejected=nullptr,same=nullptr;
    RawReadServices services;
    const auto platform=test["platform"].get<std::string>();
    services.platform=platform=="darwin"?Platform::MacOS:
      platform=="win32"?Platform::Windows:Platform::Linux;
    services.internalUser=test.value("internal",false);
    services.username=[&](){trace.push_back({"username"});
      if(test.value("usernameError",false))
        throw std::runtime_error("userInfo failed");
      return test["username"].get<std::string>();};
    services.homedir=[&](){trace.push_back({"home"});
      return test["home"].get<std::string>();};
    services.existsSync=[&](const std::string& path){
      trace.push_back({"exists",path});
      if(test.contains("existsError")&&test["existsError"]==path)
        throw std::runtime_error("exists failed");
      return test.contains("exists")&&test["exists"].value(path,false);
    };
    services.execFile=[&](const std::string& command,
        const std::vector<std::string>& args,double timeout){
      trace.push_back({"exec",command,args,timeout});
      const auto key=command=="reg"?args[1]:args.back();
      const auto response=test.contains("executions")&&
        test["executions"].contains(key)?test["executions"][key]:Json::object();
      event_loop::Deferred<RawExec> result(loop);
      result.resolve({response.value("stdout",std::string{}),
        response.value("error",false)?1:0});
      return result;
    };
    RawReader reader(loop,services);
    const auto op=test["op"].get<std::string>();
    if(op=="paths"){
      Json paths=Json::array();
      for(const auto& item:getMacOSPlistPaths(services))
        paths.push_back({{"path",item.path},{"label",item.label}});
      value=paths;
    }else{
      event_loop::Deferred<RawReadResult> raw(loop);
      if(op=="startup"){
        reader.startMdmRawRead();
        const auto first=*reader.getMdmRawReadPromise();
        reader.startMdmRawRead();
        same=first.samePromise(*reader.getMdmRawReadPromise());
        raw=first;
      }else raw=reader.fireRawRead();
      bool done=false;
      raw.then([&](const RawReadResult* result,
          const std::exception_ptr& error){
        if(error){try{std::rethrow_exception(error);}
          catch(const std::exception& caught){rejected=caught.what();}}
        else{
          Json plists=nullptr;
          if(result->plistStdouts){
            plists=Json::array();
            for(const auto& item:*result->plistStdouts)
              plists.push_back({{"stdout",item.stdoutText},
                {"label",item.label}});
          }
          value={{"plistStdouts",plists},
            {"hklmStdout",result->hklmStdout?
              Json(*result->hklmStdout):Json(nullptr)},
            {"hkcuStdout",result->hkcuStdout?
              Json(*result->hkcuStdout):Json(nullptr)}};
        }
        done=true;
      });
      for(int i=0;i<100&&!done;i++)loop.runReady();
    }
    std::cout<<value::stringify({{"value",value},{"rejected",rejected},
      {"same",same},{"trace",trace}})<<'\n';
  }
}
