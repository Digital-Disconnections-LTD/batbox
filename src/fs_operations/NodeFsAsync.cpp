#include "fs_operations/NodeFsAsync.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::fs_operations {
namespace {
template<class T,class Operation>event_loop::Deferred<T> directAsyncReturn(
    event_loop::Loop& loop,Operation&& operation){
  try{return event_loop::adoptAsyncReturn(operation());}
  catch(...){event_loop::Deferred<T> rejected(loop);
    rejected.reject(std::current_exception());return rejected;}
}
template<class T>void require(const T& callback){
  if(!callback)throw std::invalid_argument("Node async filesystem import is required");
}
}
NodeFsAsync::NodeFsAsync(NodeFsAsyncImports imports):imports_(std::move(imports)){
  if(!imports_.loop)throw std::invalid_argument("Owner event loop is required");
}
event_loop::Deferred<Json> NodeFsAsync::stat(const std::string& path)const{
  return directAsyncReturn<Json>(*imports_.loop,[&]{
    require(imports_.stat);return imports_.stat(path);});
}
event_loop::Deferred<Json> NodeFsAsync::readdir(const std::string& path)const{
  return directAsyncReturn<Json>(*imports_.loop,[&]{
    require(imports_.readdir);return imports_.readdir(path,true);});
}
event_loop::Deferred<event_loop::Unit> NodeFsAsync::unlink(
    const std::string& path)const{
  return directAsyncReturn<event_loop::Unit>(*imports_.loop,[&]{
    require(imports_.unlink);return imports_.unlink(path);});
}
event_loop::Deferred<event_loop::Unit> NodeFsAsync::rmdir(
    const std::string& path)const{
  return directAsyncReturn<event_loop::Unit>(*imports_.loop,[&]{
    require(imports_.rmdir);return imports_.rmdir(path);});
}
event_loop::Deferred<event_loop::Unit> NodeFsAsync::rm(
    const std::string& path,std::optional<Json> options)const{
  return directAsyncReturn<event_loop::Unit>(*imports_.loop,[&]{
    require(imports_.rm);return imports_.rm(path,options);});
}
event_loop::Deferred<event_loop::Unit> NodeFsAsync::mkdir(
    const std::string& path,std::optional<Json> options)const{
  event_loop::Deferred<event_loop::Unit> result(*imports_.loop);
  try{
    require(imports_.mkdir);require(imports_.errnoCode);
    Json mkdirOptions=Json::object({{"recursive",true}});
    if(options&&options->is_object())for(auto it=options->begin();it!=options->end();++it)
      mkdirOptions[it.key()]=it.value();
    auto operation=imports_.mkdir(path,mkdirOptions);
    operation.then([result,code=imports_.errnoCode](
        const event_loop::Unit*,const std::exception_ptr& error){
      if(error){
        try{
          if(code(error)!="EEXIST"){result.reject(error);return;}
        }catch(...){result.reject(std::current_exception());return;}
      }
      result.resolve({});
    });
  }catch(...){
    const auto error=std::current_exception();
    try{
      if(imports_.errnoCode&&imports_.errnoCode(error)=="EEXIST")result.resolve({});
      else result.reject(error);
    }catch(...){result.reject(std::current_exception());}
  }
  return result;
}
event_loop::Deferred<std::string> NodeFsAsync::readFile(
    const std::string& path,const std::string& encoding)const{
  return directAsyncReturn<std::string>(*imports_.loop,[&]{
    require(imports_.readFile);return imports_.readFile(path,encoding);});
}
event_loop::Deferred<event_loop::Unit> NodeFsAsync::rename(
    const std::string& source,const std::string& destination)const{
  return directAsyncReturn<event_loop::Unit>(*imports_.loop,[&]{
    require(imports_.rename);return imports_.rename(source,destination);});
}
}
