#include "paste_store/PasteStore.hpp"
#include "event_loop/Loop.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <optional>

using namespace batbox;
using namespace batbox::paste_store;
template<class T>T wait(event_loop::Loop& loop,event_loop::Deferred<T> promise){
  auto hold=loop.hold();std::optional<T> value;std::exception_ptr error;
  promise.then([&](const T* result,const std::exception_ptr& failure){
    if(failure)error=failure;else value=*result;hold.release();});
  loop.run();if(error)std::rethrow_exception(error);assert(value);return *value;
}
template<class T>event_loop::Deferred<T> ready(event_loop::Loop& loop,T value){
  event_loop::Deferred<T> result(loop);result.resolve(std::move(value));return result;
}
template<class T>event_loop::Deferred<T> failed(event_loop::Loop& loop,
    const std::string& text){
  event_loop::Deferred<T> result(loop);
  result.reject(std::make_exception_ptr(std::runtime_error(text)));return result;
}
int main(){
  const std::string payload(std::istreambuf_iterator<char>(std::cin),{});
  Json input=value::parse(payload);event_loop::Loop loop;Json calls=Json::array();
  const auto failAt=input.value("failAt",std::string{});
  int logCount=0;
  Services services;services.loop=&loop;
  services.getConfigHomeDir=[&]{calls.push_back(Json::array({"getConfigHome"}));
    return std::string("/cfg");};
  services.join=[&](const std::string& a,const std::string& b){
    calls.push_back(Json::array({"join",a,b}));return a+"/"+b;};
  services.mkdir=[&](const std::string& path,const Json& options){
    calls.push_back(Json::array({"mkdir",path,options}));
    if(failAt=="mkdir")return failed<event_loop::Unit>(loop,"mkdir failed");
    return ready(loop,event_loop::Unit{});};
  services.writeFile=[&](const std::string& path,const std::string& text,
      const Json& options){
    calls.push_back(Json::array({"writeFile",path,text,options}));
    if(failAt=="writeFile")return failed<event_loop::Unit>(loop,"writeFile failed");
    return ready(loop,event_loop::Unit{});};
  services.readFile=[&](const std::string& path,const Json& options){
    calls.push_back(Json::array({"readFile",path,options}));
    if(failAt=="readFile")return failed<std::string>(loop,"readFile failed");
    return ready(loop,input.value("readContent",std::string{}));};
  services.readdir=[&](const std::string& path){
    calls.push_back(Json::array({"readdir",path}));
    if(failAt=="readdir")return failed<std::vector<std::string>>(
      loop,"readdir failed");
    return ready(loop,input.value("files",std::vector<std::string>{}));};
  services.stat=[&](const std::string& path){
    calls.push_back(Json::array({"stat",path}));
    if(failAt=="stat")return failed<Json>(loop,"stat failed");
    const auto name=std::filesystem::path(path).filename().string();
    const auto& mtimes=value::property(input,"mtimes");
    const auto mtime=mtimes.is_object()&&mtimes.contains(name)?
      mtimes.at(name).get<double>():0.0;
    return ready(loop,Json::object({{"mtimeMs",mtime}}));};
  services.unlink=[&](const std::string& path){
    calls.push_back(Json::array({"unlink",path}));
    if(failAt=="unlink")return failed<event_loop::Unit>(loop,"unlink failed");
    return ready(loop,event_loop::Unit{});};
  services.logForDebugging=[&](const std::string& text){
    calls.push_back(Json::array({"debug",text}));
    if(failAt=="debug"&&logCount++<2)
      throw std::runtime_error("debug failed");};
  services.isENOENT=[&](const std::exception_ptr&){
    const auto code=input.value("failCode",std::string("EIO"));
    calls.push_back(Json::array({"isENOENT",code}));return code=="ENOENT";};
  services.errorToString=[](const std::exception_ptr& error){
    try{std::rethrow_exception(error);}catch(const std::exception& value){
      return std::string("Error: ")+value.what();}
  };
  std::string status="fulfilled";Json result=nullptr;
  try{
    const auto method=input.at("method").get<std::string>();
    if(method=="dir")result=getPasteStoreDir(services);
    else if(method=="path")result=getPastePath(services,
      input.at("hash").get<std::string>());
    else if(method=="hash")result=hashPastedText(
      input.at("content").get<std::string>());
    else if(method=="store")wait(loop,storePastedText(
      input.at("hash").get<std::string>(),
      input.at("content").get<std::string>(),services));
    else if(method=="retrieve"){
      const auto value=wait(loop,retrievePastedText(
        input.at("hash").get<std::string>(),services));
      result=value?Json(*value):Json(nullptr);
    }else if(method=="cleanup")wait(loop,cleanupOldPastes(
      [&]{calls.push_back(Json::array({"getTime"}));
        return input.at("cutoff").get<double>();},services));
  }catch(const std::exception& error){status="rejected";result=error.what();}
  std::cout<<value::stringify(Json::object({{"status",status},
    {"result",result},{"calls",calls}}))<<'\n';
}
