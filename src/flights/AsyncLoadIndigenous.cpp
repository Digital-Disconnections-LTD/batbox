#include "runtime/AsyncLaunch.hpp"
#include "flights/AsyncLoad.hpp"
#include "flights/scanning/Scanning.hpp"
#include <thread>

namespace batbox::flights {
namespace {
template<class Operation>auto dispatch(event_loop::Loop& loop,Operation operation){
  using T=decltype(operation());event_loop::Deferred<T> result(loop);
  auto keep=std::make_shared<event_loop::Loop::KeepAlive>(loop.hold());const auto dispatcher=loop.dispatcher();
  try{::batbox::runtime::contextThread([operation=std::move(operation),result,keep,dispatcher]{
    std::shared_ptr<T> value;std::exception_ptr error;
    try{value=std::make_shared<T>(operation());}catch(...){error=std::current_exception();}
    try{dispatcher.postMission([result,keep,value,error]{
      if(error)result.reject(error);else result.resolve(std::move(*value));keep->release();
    });}catch(...){keep->release();}
  }).detach();}catch(...){keep->release();result.reject(std::current_exception());}
  return result;
}
}
void bindIndigenousAsyncFileReads(event_loop::Loop& loop,AsyncLoadServices& services){
  LoadServices indigenous;bindIndigenousFileReads(indigenous);
  services.stat=[&loop,stat=indigenous.stat](const auto& path){return dispatch(loop,[stat,path]{return stat(path);});};
  services.readFile=[&loop,read=indigenous.readFile](const auto& path){return dispatch(loop,[read,path]{return read(path);});};
  services.readTranscriptForLoad=[&loop](const auto& path,std::size_t size){
    return dispatch(loop,[path,size]{return scanning::readTranscriptForLoad(path,size);});};
  services.scanPreBoundaryMetadata=[&loop](const auto& path,std::size_t offset){
    return dispatch(loop,[path,offset]{return scanning::scanPreBoundaryMetadata(path,offset);});};
  services.walkChainBeforeParse=scanning::walkChainBeforeParse;
}
} // namespace batbox::flights
