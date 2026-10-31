#include "fs_operations/NodeFsAsync.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

using namespace batbox;
struct MockError:std::runtime_error{
  MockError(std::string message,std::string code):
    std::runtime_error(std::move(message)),code(std::move(code)){}
  std::string code;
};
int main(){
  const std::string raw(std::istreambuf_iterator<char>(std::cin),{});
  const auto input=value::parse(raw);
  event_loop::Loop loop;
  Json calls=Json::array(),result=nullptr;
  std::string status="fulfilled";
  auto failed=[&](const std::string& name){
    return input.value("failAt",std::string())==name;};
  auto error=[&](const std::string& name){return std::make_exception_ptr(
    MockError(name+" failed",input.value("failCode",std::string("EIO"))));};
  fs_operations::NodeFsAsyncImports imports;
  imports.loop=&loop;
  imports.stat=[&](const std::string& path){
    calls.push_back(Json::array({"stat",path}));
    event_loop::Deferred<Json> promise(loop);
    if(failed("stat"))promise.reject(error("stat"));
    else promise.resolve(Json::object({{"size",7}}));
    return promise;};
  imports.readdir=[&](const std::string& path,bool withTypes){
    calls.push_back(Json::array({"readdir",path,
      Json::object({{"withFileTypes",withTypes}})}));
    event_loop::Deferred<Json> promise(loop);
    if(failed("readdir"))promise.reject(error("readdir"));
    else promise.resolve(Json::array({Json::object({{"name","x"}})}));
    return promise;};
  auto unit=[&](const std::string& name,const Json& arguments){
    Json call=Json::array({name});for(const auto& arg:arguments)call.push_back(arg);
    calls.push_back(std::move(call));
    event_loop::Deferred<event_loop::Unit> promise(loop);
    if(failed(name))promise.reject(error(name));else promise.resolve({});
    return promise;};
  imports.unlink=[&](const std::string& path){return unit("unlink",Json::array({path}));};
  imports.rmdir=[&](const std::string& path){return unit("rmdir",Json::array({path}));};
  imports.rm=[&](const std::string& path,const std::optional<Json>& options){
    return unit("rm",Json::array({path,options?*options:Json(nullptr)}));};
  imports.mkdir=[&](const std::string& path,const Json& options){
    return unit("mkdir",Json::array({path,options}));};
  imports.readFile=[&](const std::string& path,const std::string& encoding){
    calls.push_back(Json::array({"readFile",path,
      Json::object({{"encoding",encoding}})}));
    event_loop::Deferred<std::string> promise(loop);
    if(failed("readFile"))promise.reject(error("readFile"));
    else promise.resolve("contents");
    return promise;};
  imports.rename=[&](const std::string& a,const std::string& b){
    return unit("rename",Json::array({a,b}));};
  imports.errnoCode=[](const std::exception_ptr& error)->std::optional<std::string>{
    try{std::rethrow_exception(error);}catch(const MockError& value){return value.code;}
    catch(...){return std::nullopt;}};
  fs_operations::NodeFsAsync node(std::move(imports));
  const auto args=input.value("args",Json::array());
  const auto a=[&](std::size_t at){return args.at(at).get<std::string>();};
  const auto method=input.at("method").get<std::string>();
  auto observe=[&](const auto& promise){
    promise.then([&](const auto* value,const std::exception_ptr& exception){
      if(exception){status="rejected";try{std::rethrow_exception(exception);}
        catch(const std::exception& caught){result=caught.what();}}
      else if constexpr(std::is_same_v<std::decay_t<decltype(*value)>,event_loop::Unit>)
        result=nullptr;
      else result=*value;
      calls.push_back(Json::array({"resolved"}));
    });
  };
  if(method=="stat")observe(node.stat(a(0)));
  if(method=="readdir")observe(node.readdir(a(0)));
  if(method=="unlink")observe(node.unlink(a(0)));
  if(method=="rmdir")observe(node.rmdir(a(0)));
  if(method=="rm"){
    std::optional<Json> options;if(args.size()>1)options=args[1];
    observe(node.rm(a(0),options));}
  if(method=="mkdir"){
    std::optional<Json> options;if(args.size()>1)options=args[1];
    observe(node.mkdir(a(0),options));}
  if(method=="readFile")observe(node.readFile(a(0),args.at(1).at("encoding")));
  if(method=="rename")observe(node.rename(a(0),a(1)));
  loop.queueMicrotask([&]{calls.push_back(Json::array({"tick"}));});
  loop.runReady();
  std::cout<<value::stringify(Json::object({{"status",status},{"result",result},
    {"calls",calls}}));
}
