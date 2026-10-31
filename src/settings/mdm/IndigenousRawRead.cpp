#include "settings/mdm/RawRead.hpp"
#include "process/ExecFileNoThrow.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/IndigenousPath.hpp"
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#ifndef _WIN32
#include <pwd.h>
#include <unistd.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vector>
#endif
#include <stdexcept>
#include <system_error>

namespace batbox::settings::mdm {
namespace {
std::string passwdField(bool username){
#ifdef _WIN32
  const wchar_t* name=username?L"USERNAME":L"USERPROFILE";
  const auto size=::GetEnvironmentVariableW(name,nullptr,0);
  if(!size)throw std::system_error(static_cast<int>(::GetLastError()),
    std::system_category(),"MDM userInfo");
  std::wstring value(size,L'\0');
  const auto copied=::GetEnvironmentVariableW(name,value.data(),size);
  if(!copied||copied>=size)throw std::system_error(static_cast<int>(::GetLastError()),
    std::system_category(),"MDM userInfo");
  value.resize(copied);
  return runtime::pathToUtf8(std::filesystem::path(value));
#else
  long count=::sysconf(_SC_GETPW_R_SIZE_MAX);
  if(count<1024)count=16384;
  std::string buffer(static_cast<std::size_t>(count),'\0');
  struct passwd record{},*found=nullptr;
  const int code=::getpwuid_r(::getuid(),&record,buffer.data(),
    buffer.size(),&found);
  if(code||!found)
    throw std::system_error(code?code:ENOENT,std::generic_category(),
      "MDM userInfo");
  const char* field=username?record.pw_name:record.pw_dir;
  if(!field)throw std::system_error(ENOENT,std::generic_category(),
    "MDM userInfo");
  return field;
#endif
}
}
Platform indigenousPlatform(){
#if defined(__APPLE__)
  return Platform::MacOS;
#elif defined(_WIN32)
  return Platform::Windows;
#else
  return Platform::Linux;
#endif
}
RawReadServices indigenousRawReadServices(event_loop::Loop& loop,
    Platform platform,bool internalUser,Names names){
  RawReadServices services;
  services.platform=platform;services.internalUser=internalUser;
  services.names=std::move(names);
  services.username=[](){return passwdField(true);};
  services.homedir=[](){
#ifdef _WIN32
    return passwdField(false);
#else
    if(const char* home=std::getenv("HOME");home&&*home)return std::string(home);
    return passwdField(false);
#endif
  };
  services.existsSync=[](const std::string& path){
    std::error_code error;
    return std::filesystem::exists(runtime::pathFromUtf8(path),error)&&!error;
  };
  auto indigenous=process::indigenousExeca(loop);
  services.execFile=[&loop,indigenous=std::move(indigenous)](
      const std::string& command,const std::vector<std::string>& args,
      double timeout){
    event_loop::Deferred<RawExec> output(loop);
    process::ExecOptions options;options.timeout=timeout;
    try{indigenous({command,args,options}).then([output](
        const process::ExecaResult* result,const std::exception_ptr& error){
      if(error){output.resolve({"",1});return;}
      output.resolve({result->stdoutText,result->failed?1:0});
    });}catch(...){output.resolve({"",1});}
    return output;
  };
  return services;
}
}
