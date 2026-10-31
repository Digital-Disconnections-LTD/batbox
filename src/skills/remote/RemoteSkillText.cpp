#include "skills/remote/RemoteSkill.hpp"
#include <filesystem>

namespace batbox::skills::remote {
std::string remoteSkillDirectory(const std::string& path,bool isWindows){
  auto directory=std::filesystem::path(path).parent_path().string();
  if(directory.empty())directory=".";
  if(isWindows)for(auto& ch:directory)if(ch=='\\')ch='/';
  return directory;
}
std::string replaceLiteralWithJsSubstitution(const std::string& input,
  const std::string& marker,const std::string& replacement){
  if(marker.empty())return input;
  std::string output;
  std::size_t from=0;
  for(std::size_t match;(match=input.find(marker,from))!=std::string::npos;
      from=match+marker.size()){
    output+=input.substr(from,match-from);
    for(std::size_t i=0;i<replacement.size();++i){
      if(replacement[i]!='$'||i+1>=replacement.size()){
        output+=replacement[i];continue;
      }
      const char next=replacement[i+1];
      if(next=='$'){output+='$';++i;}
      else if(next=='&'){output+=marker;++i;}
      else if(next=='`'){output+=input.substr(0,match);++i;}
      else if(next=='\''){output+=input.substr(match+marker.size());++i;}
      else output+='$';
    }
  }
  output+=input.substr(from);
  return output;
}
}
