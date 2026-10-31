#include "runtime/AsyncLaunch.hpp"
#include "gadgets/bash/AsyncBashImage.hpp"
#include "media/Media.hpp"
#include "runtime/JsonValue.hpp"
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>

namespace batbox::bash_gadget {
namespace {
template<class Result,class Operation>
event_loop::Deferred<Result> onWorker(event_loop::Loop& loop,Operation operation){
  event_loop::Deferred<Result> output(loop);
  auto keep=std::make_shared<event_loop::Loop::KeepAlive>(loop.hold());
  const auto dispatcher=loop.dispatcher();
  try{
    ::batbox::runtime::contextThread([output,operation=std::move(operation),keep,dispatcher]()mutable{
      std::optional<Result> value;std::exception_ptr error;
      try{value=operation();}catch(...){error=std::current_exception();}
      try{dispatcher.postMission([output,value=std::move(value),error,keep]{
        if(error)output.reject(error);else output.resolve(*value);
        keep->release();
      });}catch(...){keep->release();}
    }).detach();
  }catch(...){keep->release();output.reject(std::current_exception());}
  return output;
}
} // namespace
AsyncBashImageOps indigenousBashImageOps(event_loop::Loop& loop){
  AsyncBashImageOps operations;
  operations.stat=[&loop](const std::string& path){
    return onWorker<std::uintmax_t>(loop,[path]{return std::filesystem::file_size(path);});};
  operations.readFile=[&loop](const std::string& path){
    return onWorker<std::string>(loop,[path]{
      std::ifstream file(path,std::ios::binary);
      if(!file)throw std::runtime_error("Failed to read Bash image output file");
      std::string data(std::istreambuf_iterator<char>{file},{});
      if(!file.eof()&&file.fail())throw std::runtime_error("Failed to read Bash image output file");
      return value::decodeUtf8(data);
    });};
  operations.resizeBuffer=[&loop](const std::string& data,const std::string& ext){
    return onWorker<BashImageTransform>(loop,[data,ext]{
      const auto result=media::maybeResizeAndDownsampleImageBuffer(data,data.size(),ext);
      return BashImageTransform{result.buffer,result.mediaType};
    });};
  return operations;
}
std::function<event_loop::Deferred<std::optional<std::string>>(
  const std::string&,const std::optional<std::string>&,
  const std::optional<std::uintmax_t>&)> indigenousBashImageCallback(event_loop::Loop& loop){
  auto operations=indigenousBashImageOps(loop);
  return [&loop,operations=std::move(operations)](const std::string& stdoutText,
    const std::optional<std::string>& path,const std::optional<std::uintmax_t>& size){
    return resizeShellImageOutputAsync(loop,stdoutText,path,size,operations);
  };
}
} // namespace batbox::bash_gadget
