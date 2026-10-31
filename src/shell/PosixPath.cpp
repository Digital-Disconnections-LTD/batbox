#include "shell/PosixPath.hpp"
#include <vector>

namespace batbox::shell {
std::string joinPosixPath(const std::string& first,const std::string& second){
  std::string joined;
  if(!first.empty())joined=first;
  if(!second.empty()){
    if(!joined.empty())joined+='/';
    joined+=second;
  }
  if(joined.empty())return ".";
  const bool absolute=joined[0]=='/';
  const bool trailing=joined.back()=='/';
  std::vector<std::string> segments;
  for(std::size_t start=0;start<joined.size();){
    while(start<joined.size()&&joined[start]=='/')++start;
    if(start>=joined.size())break;
    auto end=joined.find('/',start);
    if(end==std::string::npos)end=joined.size();
    const auto segment=joined.substr(start,end-start);
    if(segment==".."){
      if(!segments.empty()&&segments.back()!="..")segments.pop_back();
      else if(!absolute)segments.push_back("..");
    }else if(segment!=".")segments.push_back(segment);
    start=end;
  }
  std::string result=absolute?"/":"";
  for(const auto& segment:segments){
    if(!result.empty()&&result.back()!='/')result+='/';
    result+=segment;
  }
  if(result.empty())result=".";
  if(trailing&&result.back()!='/')result+='/';
  return result;
}
} // namespace batbox::shell
