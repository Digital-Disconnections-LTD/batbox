#pragma once
#include <optional>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>

namespace batbox::runtime {
inline std::string wideToUtf8(const std::wstring& wide){
  if(wide.empty())return {};
  const int needed=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,
    wide.data(),static_cast<int>(wide.size()),nullptr,0,nullptr,nullptr);
  if(!needed)throw std::runtime_error("Invalid UTF-16 process text");
  std::string result(static_cast<std::size_t>(needed),'\0');
  const int written=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,
    wide.data(),static_cast<int>(wide.size()),result.data(),needed,nullptr,nullptr);
  if(written!=needed)throw std::runtime_error("Cannot convert UTF-16 process text");
  return result;
}
inline std::optional<std::string> environmentUtf8Optional(const char* name){
  const std::wstring wideName(name,name+std::char_traits<char>::length(name));
  SetLastError(ERROR_SUCCESS);
  const DWORD size=GetEnvironmentVariableW(wideName.c_str(),nullptr,0);
  if(!size){
    const DWORD error=GetLastError();
    if(error==ERROR_ENVVAR_NOT_FOUND)return std::nullopt;
    if(error==ERROR_SUCCESS)return std::string{};
    throw std::runtime_error("Cannot read Windows environment variable "+
      std::string(name));
  }
  std::wstring buffer(size,L'\0');
  const DWORD length=GetEnvironmentVariableW(wideName.c_str(),buffer.data(),size);
  if(length>=size)throw std::runtime_error("Environment changed while reading "+
    std::string(name));
  buffer.resize(length);
  return wideToUtf8(buffer);
}
inline std::string environmentUtf8(const char* name){
  return environmentUtf8Optional(name).value_or(std::string{});
}
} // namespace batbox::runtime
#endif
