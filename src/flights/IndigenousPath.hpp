#pragma once
#include "runtime/IndigenousPath.hpp"

namespace batbox::flights {
inline std::filesystem::path indigenousPath(const std::string& utf8){
  return runtime::pathFromUtf8(utf8);
}
// For opening, creating and measuring files only: never turn the result back
// into a stored or displayed path.
inline std::filesystem::path indigenousFilePath(const std::string& utf8){
  auto path=runtime::pathFromUtf8(utf8);
#if defined(_WIN32)
  // Bat transcripts nest a project directory, a Flight ID and an agent ID under
  // the state directory, which easily passes MAX_PATH. Win32 rejects such paths
  // unless they carry the extended-length prefix, and that prefix needs an
  // absolute, normalized path with backslashes.
  constexpr std::size_t shortestDirectoryLimit=248;
  const std::wstring prefix=L"\\\\?\\";
  if(path.native().size()<shortestDirectoryLimit||!path.is_absolute()||
     path.native().rfind(prefix,0)==0)return path;
  auto normal=path.lexically_normal();
  normal.make_preferred();
  const auto& text=normal.native();
  if(text.rfind(L"\\\\",0)==0)return std::filesystem::path(prefix+L"UNC\\"+text.substr(2));
  return std::filesystem::path(prefix+text);
#else
  return path;
#endif
}
} // namespace batbox::flights
