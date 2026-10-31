#include "file_state_stash/FileStateStash.hpp"

namespace batbox::file_state_stash {
std::string normalizeKey(const std::string& path){
  if(path.empty())return ".";
  const bool absolute=path.front()=='/';
  const bool trailing=path.back()=='/';
  std::vector<std::string> segments;
  for(std::size_t start=0;start<path.size();){
    while(start<path.size()&&path[start]=='/')++start;
    if(start==path.size())break;
    const auto slash=path.find('/',start);
    const auto end=slash==std::string::npos?path.size():slash;
    const auto segment=path.substr(start,end-start);
    if(segment==".."){
      if(!segments.empty()&&segments.back()!="..")segments.pop_back();
      else if(!absolute)segments.push_back(segment);
    }else if(segment!=".")segments.push_back(segment);
    start=end;
  }
  std::string normalized=absolute?"/":"";
  for(const auto& segment:segments){
    if(!normalized.empty()&&normalized.back()!='/')normalized+='/';
    normalized+=segment;
  }
  if(normalized.empty())normalized=".";
  if(trailing&&normalized.back()!='/')normalized+='/';
  return normalized;
}
} // namespace batbox::file_state_stash
