#include "cli/structured/IndigenousProcessLine.hpp"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#include <windows.h>
#endif

namespace batbox::cli::structured {
#ifdef _WIN32
namespace {
std::wstring utf8ToWide(const std::string& text){
  if(text.empty())return {};
  const int needed=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,
    text.data(),static_cast<int>(text.size()),nullptr,0);
  if(!needed)throw std::runtime_error("Invalid UTF-8 environment text");
  std::wstring wide(static_cast<std::size_t>(needed),L'\0');
  if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),
      static_cast<int>(text.size()),wide.data(),needed)!=needed)
    throw std::runtime_error("Cannot convert UTF-8 environment text");
  return wide;
}
}
#endif
void bindIndigenousProcessLineOS(ProcessLineServices& services){
  services.setEnvironment=[](const std::string& key,const std::string& value){
#ifdef _WIN32
    if(key.empty()||key.find('=')!=std::string::npos||
       key.find('\0')!=std::string::npos||value.find('\0')!=std::string::npos)
      throw std::invalid_argument("Invalid environment variable name or value");
    const auto name=utf8ToWide(key),contents=utf8ToWide(value);
    const int crt=_wputenv_s(name.c_str(),contents.c_str());
    if(crt!=0)throw std::system_error(crt,std::generic_category(),
      "_wputenv_s("+key+")");
    // _wputenv_s removes empty values. The Win32 environment retains the
    // empty-string value for Unicode child processes.
    if(!SetEnvironmentVariableW(name.c_str(),contents.c_str()))
      throw std::system_error(static_cast<int>(GetLastError()),
        std::system_category(),"SetEnvironmentVariableW("+key+")");
#else
    if(::setenv(key.c_str(),value.c_str(),1)!=0)
      throw std::system_error(errno,std::generic_category(),
                              "setenv("+key+")");
#endif
  };
  services.fatalInput=[](const std::string& message){
    std::fwrite(message.data(),1,message.size(),stderr);
    std::fwrite("\n",1,1,stderr);
    std::fflush(stderr);
    std::exit(1);
  };
}
}
