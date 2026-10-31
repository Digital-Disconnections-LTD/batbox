#include "auth/storage/Storage.hpp"
#include "runtime/JsonValue.hpp"
#include <cerrno>
#include <iostream>
#include <stdexcept>
#include <system_error>

namespace {
using batbox::Json;
int errnoCode(const std::string& value){
  return value=="EEXIST"?EEXIST:
    value=="ENOENT"?ENOENT:EACCES;
}
template<class T>T await(batbox::event_loop::Loop& loop,
    batbox::event_loop::Deferred<T> promise){
  std::optional<T> value;std::exception_ptr error;
  promise.then([&](const T* result,const std::exception_ptr& failure){
    if(failure)error=failure;else value=*result;
  });
  loop.run();
  if(error)std::rethrow_exception(error);
  if(!value)throw std::runtime_error("Storage promise did not settle");
  return *value;
}
}
int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    Json trace=Json::array();
    event_loop::Loop loop;
    auto fake=[&](const std::string& name){
      auth::storage::Storage result;
      result.name=name;
      result.read=[&,name]()->std::optional<Json>{
        trace.push_back(name+"Read");
        if(test.value(name+"ReadError",false))
          throw std::runtime_error(name+" read failed");
        if(!test.contains(name+"Data")||test[name+"Data"].is_null())
          return std::nullopt;
        return test[name+"Data"];
      };
      result.readAsync=[&,name](){
        trace.push_back(name+"ReadAsync");
        event_loop::Deferred<std::optional<Json>> output(loop);
        if(test.value(name+"ReadError",false))
          output.reject(std::make_exception_ptr(
            std::runtime_error(name+" read failed")));
        else output.resolve(test.contains(name+"Data")&&
            !test[name+"Data"].is_null()
          ?std::optional<Json>(test[name+"Data"]):std::nullopt);
        return output;
      };
      result.update=[&,name](const Json& data){
        trace.push_back(Json::array({name+"Update",data}));
        if(test.value(name+"UpdateError",false))
          throw std::runtime_error(name+" update failed");
        return test.value(name+"Status",Json{{"success",true}});
      };
      result.remove=[&,name](){
        trace.push_back(name+"Delete");
        if(test.value(name+"DeleteError",false))
          throw std::runtime_error(name+" delete failed");
        return test.value(name+"DeleteResult",true);
      };
      return result;
    };
    auth::storage::PlaintextServices callbacks;
    callbacks.configHome=[&](){trace.push_back("home");
      return std::string("/config");};
    callbacks.readFileSync=[&](const std::string& path){
      trace.push_back(Json::array({"read",path}));
      if(test.value("readError",false))throw std::system_error(
        EACCES,std::generic_category(),"read failed");
      return test.value("fileText",std::string("{}"));
    };
    callbacks.readFileAsync=[&](const std::string& path){
      trace.push_back(Json::array({"readAsync",path}));
      event_loop::Deferred<std::string> output(loop);
      if(test.value("readError",false))output.reject(
        std::make_exception_ptr(std::system_error(EACCES,
          std::generic_category(),"read failed")));
      else output.resolve(test.value("fileText",std::string("{}")));
      return output;
    };
    callbacks.mkdirSync=[&](const std::string& path){
      trace.push_back(Json::array({"mkdir",path}));
      if(test.contains("mkdirError"))throw std::system_error(
        errnoCode(test["mkdirError"].get<std::string>()),
        std::generic_category(),"mkdir failed");
    };
    callbacks.writeFileSync=[&](const std::string& path,
        const std::string& data){
      trace.push_back(Json::array({"write",path,data}));
      if(test.value("writeError",false))throw std::system_error(
        EACCES,std::generic_category(),"write failed");
    };
    callbacks.chmodSync=[&](const std::string& path,int mode){
      trace.push_back(Json::array({"chmod",path,mode}));
      if(test.value("chmodError",false))throw std::system_error(
        EACCES,std::generic_category(),"chmod failed");
    };
    callbacks.unlinkSync=[&](const std::string& path){
      trace.push_back(Json::array({"unlink",path}));
      if(test.contains("unlinkError"))throw std::system_error(
        errnoCode(test["unlinkError"].get<std::string>()),
        std::generic_category(),"unlink failed");
    };
    Json value;
    try{
      const auto kind=test.value("kind",std::string("plain"));
      auto storage=kind=="fallback"
        ?auth::storage::makeFallbackStorage(loop,fake("primary"),
          fake("secondary"))
        :auth::storage::makePlaintextStorage(loop,callbacks);
      if(kind=="select")storage=auth::storage::selectStorage(loop,
        test.value("platform",std::string("linux")),storage,fake("keychain"));
      const auto op=test.value("op",std::string());
      if(op=="read"){
        const auto data=storage.read();value=data?*data:Json(nullptr);
      }else if(op=="readAsync"){
        const auto data=await(loop,storage.readAsync());
        value=data?*data:Json(nullptr);
      }else if(op=="update")value=storage.update(
        test.value("data",Json::object()));
      else if(op=="delete")value=storage.remove();
      else value=storage.name;
    }catch(const std::exception& error){value=Json{{"error",error.what()}};}
    std::cout<<value::stringify(Json{{"value",value},{"trace",trace}})<<'\n';
  }
}
