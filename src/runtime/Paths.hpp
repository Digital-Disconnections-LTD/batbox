#pragma once
#include <string>
namespace batbox::value {
// Node path.posix.dirname adapter for POSIX-style path strings.
inline std::string dirname(const std::string& path) {
  if(path.empty())return ".";
  const bool rooted=path[0]=='/';bool matchedSlash=true;std::size_t end=std::string::npos;
  for(std::size_t i=path.size();i-->1;) {
    if(path[i]=='/'){if(!matchedSlash){end=i;break;}}
    else matchedSlash=false;
  }
  if(end==std::string::npos)return rooted ? "/" : ".";
  if(rooted && end==1)return "//";
  return path.substr(0,end);
}
}
