#include "fs_operations/NodeFsSync.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
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
  Json calls=Json::array(),result=nullptr;
  std::string status="fulfilled";
  auto fail=[&](const std::string& operation){
    if(input.value("failAt",std::string())==operation)
      throw MockError(operation+" failed",input.value("failCode",std::string("EIO")));
  };
  auto text=[&]{
    auto value=input.value("text",std::string("contents"));
    if(value.empty())value="contents";
    return value;
  };
  fs_operations::NodeFsSyncImports imports;
  imports.beginSlowLogging=[&](const std::vector<std::string>& strings,
      const std::vector<std::optional<Json>>& values){
    Json mapped=Json::array();for(const auto& value:values)
      mapped.push_back(value?*value:Json(nullptr));
    calls.push_back(Json::array({"begin",strings,mapped}));
    return std::function<void()>([&]{
      calls.push_back(Json::array({"dispose"}));
      if(input.value("disposeThrow",false))throw MockError("dispose failed","EIO");
    });
  };
  imports.cwd=[&]{return input.value("cwd",std::string("/work"));};
  imports.existsSync=[&](const std::string& path){
    calls.push_back(Json::array({"exists",path}));fail("exists");
    return input.value("exists",false);};
  imports.statSync=[&](const std::string& path){
    calls.push_back(Json::array({"stat",path}));fail("stat");
    auto size=input.value("size",3);if(!size)size=3;
    return Json::object({{"size",size}});};
  imports.lstatSync=[&](const std::string& path){
    calls.push_back(Json::array({"lstat",path}));fail("lstat");
    auto size=input.value("size",3);if(!size)size=3;
    return Json::object({{"size",size}});};
  imports.readFileSync=[&](const std::string& path,const std::string& encoding){
    calls.push_back(Json::array({"readFile",path,
      Json::object({{"encoding",encoding}})}));fail("readFile");return text();};
  imports.readFileBytesSync=[&](const std::string& path){
    calls.push_back(Json::array({"readFile",path,nullptr}));fail("readFile");
    const auto value=text();return std::vector<unsigned char>(value.begin(),value.end());};
  imports.openSync=[&](const std::string& path,const std::string& flag,
      const std::optional<Json>& mode){
    calls.push_back(Json::array({"open",path,flag,mode?*mode:Json(nullptr)}));
    fail("open");return input.value("fd",7);};
  imports.readSync=[&](int fd,std::vector<unsigned char>& buffer,
      std::size_t offset,std::size_t length,std::size_t position){
    calls.push_back(Json::array({"read",fd,offset,length,position}));fail("read");
    const auto value=input.value("text",std::string("abc"));
    const auto amount=std::min({value.size(),length,
      input.value("bytesRead",length)});
    for(std::size_t i=0;i<amount;++i)buffer[offset+i]=
      static_cast<unsigned char>(value[i]);
    return amount;
  };
  imports.closeSync=[&](int fd){calls.push_back(Json::array({"close",fd}));fail("close");};
  imports.appendFdSync=[&](int fd,const std::string& data){
    calls.push_back(Json::array({"append",fd,data}));fail("append");};
  imports.appendPathSync=[&](const std::string& path,const std::string& data){
    calls.push_back(Json::array({"append",path,data}));fail("append");};
  imports.errnoCode=[](const std::exception_ptr& error)->std::optional<std::string>{
    try{std::rethrow_exception(error);}catch(const MockError& value){return value.code;}
    catch(...){return std::nullopt;}
  };
  imports.copyFileSync=[&](const std::string& a,const std::string& b){
    calls.push_back(Json::array({"copy",a,b}));fail("copy");};
  imports.unlinkSync=[&](const std::string& a){
    calls.push_back(Json::array({"unlink",a}));fail("unlink");};
  imports.renameSync=[&](const std::string& a,const std::string& b){
    calls.push_back(Json::array({"rename",a,b}));fail("rename");};
  imports.linkSync=[&](const std::string& a,const std::string& b){
    calls.push_back(Json::array({"link",a,b}));fail("link");};
  imports.symlinkSync=[&](const std::string& a,const std::string& b,
      const std::optional<std::string>& type){
    calls.push_back(Json::array({"symlink",a,b,type?Json(*type):Json(nullptr)}));
    fail("symlink");};
  imports.readlinkSync=[&](const std::string& a){
    calls.push_back(Json::array({"readlink",a}));fail("readlink");
    return input.value("target",std::string("/dest"));};
  imports.realpathSync=[&](const std::string& a){
    calls.push_back(Json::array({"realpath",a}));fail("realpath");
    return input.value("realpath",a);};
  imports.mkdirSync=[&](const std::string& a,const Json& options){
    calls.push_back(Json::array({"mkdir",a,options}));fail("mkdir");};
  imports.readdirSync=[&](const std::string& a,bool withTypes){
    calls.push_back(Json::array({"readdir",a,
      Json::object({{"withFileTypes",withTypes}})}));fail("readdir");
    return input.value("entries",Json::array());};
  imports.readdirStringSync=[&](const std::string& a){
    calls.push_back(Json::array({"readdir",a,nullptr}));fail("readdir");
    return input.value("entries",Json::array()).get<std::vector<std::string>>();};
  imports.rmdirSync=[&](const std::string& a){
    calls.push_back(Json::array({"rmdir",a}));fail("rmdir");};
  imports.rmSync=[&](const std::string& a,const std::optional<Json>& options){
    calls.push_back(Json::array({"rm",a,options?*options:Json(nullptr)}));fail("rm");};

  fs_operations::NodeFsSync node(std::move(imports));
  const auto method=input.at("method").get<std::string>();
  const auto args=input.value("args",Json::array());
  const auto a=[&](std::size_t index){return args.at(index).get<std::string>();};
  try{
    if(method=="cwd")result=node.cwd();
    if(method=="existsSync")result=node.existsSync(a(0));
    if(method=="statSync")result=node.statSync(a(0));
    if(method=="lstatSync")result=node.lstatSync(a(0));
    if(method=="readFileSync")result=node.readFileSync(a(0),args.at(1).at("encoding"));
    if(method=="readFileBytesSync"){
      const auto data=node.readFileBytesSync(a(0));
      result=Json::object({{"type","Buffer"},{"data",data}});
    }
    if(method=="readSync"){
      const auto read=node.readSync(a(0),args.at(1).at("length"));
      result=Json::object({{"buffer",Json::object({{"type","Buffer"},
        {"data",read.buffer}})},{"bytesRead",read.bytesRead}});
    }
    if(method=="appendFileSync"){
      std::optional<Json> options;if(args.size()>2)options=args[2];
      node.appendFileSync(a(0),a(1),options);
    }
    if(method=="copyFileSync")node.copyFileSync(a(0),a(1));
    if(method=="unlinkSync")node.unlinkSync(a(0));
    if(method=="renameSync")node.renameSync(a(0),a(1));
    if(method=="linkSync")node.linkSync(a(0),a(1));
    if(method=="symlinkSync"){
      std::optional<std::string> type;if(args.size()>2&&!args[2].is_null())
        type=args[2].get<std::string>();
      node.symlinkSync(a(0),a(1),type);
    }
    if(method=="readlinkSync")result=node.readlinkSync(a(0));
    if(method=="realpathSync")result=node.realpathSync(a(0));
    if(method=="mkdirSync"){
      std::optional<Json> options;if(args.size()>1)options=args[1];
      node.mkdirSync(a(0),options);
    }
    if(method=="readdirSync")result=node.readdirSync(a(0));
    if(method=="readdirStringSync")result=node.readdirStringSync(a(0));
    if(method=="isDirEmptySync")result=node.isDirEmptySync(a(0));
    if(method=="rmdirSync")node.rmdirSync(a(0));
    if(method=="rmSync"){
      std::optional<Json> options;if(args.size()>1)options=args[1];
      node.rmSync(a(0),options);
    }
  }catch(const std::exception& error){status="rejected";result=error.what();}
  std::cout<<value::stringify(Json::object({{"status",status},{"result",result},
    {"calls",calls}}));
}
