#include "runtime/AsyncLaunch.hpp"
#include "flights/IndigenousPath.hpp"
#include "flights/async_project/AsyncProject.hpp"
#include <cerrno>
#include <memory>
#include <filesystem>
#include <stdexcept>
#include <thread>
#include <utility>

namespace batbox::flights::async_project {
std::function<Completion(const std::string&)> indigenousStatFile(event_loop::Loop& loop){
  return [&loop](const std::string& path){
    Completion result(loop);
    auto lease=std::make_shared<event_loop::Loop::KeepAlive>(loop.hold());
    const auto dispatcher=loop.dispatcher();
    try{
      ::batbox::runtime::contextThread([path,result,lease,dispatcher]{
        std::exception_ptr error;
        std::error_code code;
        (void)std::filesystem::status(indigenousPath(path),code);
        if(code)error=std::make_exception_ptr(std::filesystem::filesystem_error(
          "stat",indigenousPath(path),code));
        try{dispatcher.postMission([result,lease,error]{
          if(error)result.reject(error);else result.resolve({});
          lease->release();
        });}catch(...){lease->release();}
      }).detach();
    }catch(...){lease->release();result.reject(std::current_exception());}
    return result;
  };
}
} // namespace batbox::flights::async_project
