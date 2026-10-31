#include "repository/Detect.hpp"
#include <regex>

namespace batbox::repository {
Json ParsedRepository::toJson()const{return {{"host",host},{"owner",owner},{"name",name}};}
bool looksLikeRealHostname(const std::string& host){
  const auto last=host.rfind('.');
  if(last==std::string::npos||last+1==host.size())return false;
  for(std::size_t i=last+1;i<host.size();++i)
    if(!((host[i]>='a'&&host[i]<='z')||(host[i]>='A'&&host[i]<='Z')))return false;
  return true;
}
RepositoryRef parseGitRemote(const std::string& input){
  const auto trimmed=value::trim(input);std::smatch match;
  // Preserve explicit case and port matching rules; this repository parser has
  // different semantics from a general URL constructor.
  static const std::regex ssh(R"(^git@([^:]+):([^/]+)/([^/]+?)(?:\.git)?$)");
  if(std::regex_match(trimmed,match,ssh)&&!match[1].str().empty()&&
      !match[2].str().empty()&&!match[3].str().empty()){
    if(!looksLikeRealHostname(match[1]))return {};
    return std::make_shared<ParsedRepository>(ParsedRepository{match[1],match[2],match[3]});
  }
  static const std::regex url(R"(^(https?|ssh|git)://(?:[^@]+@)?([^/:]+(?::\d+)?)/([^/]+)/([^/]+?)(?:\.git)?$)");
  if(std::regex_match(trimmed,match,url)&&!match[1].str().empty()&&
      !match[2].str().empty()&&!match[3].str().empty()&&!match[4].str().empty()){
    const auto protocol=match[1].str(),hostWithPort=match[2].str();
    const auto hostWithoutPort=hostWithPort.substr(0,hostWithPort.find(':'));
    if(!looksLikeRealHostname(hostWithoutPort))return {};
    return std::make_shared<ParsedRepository>(ParsedRepository{
      protocol=="https"||protocol=="http"?hostWithPort:hostWithoutPort,match[3],match[4]});
  }
  return {};
}
std::optional<std::string> parseGitHubRepository(const std::string& input,const Debug& debug){
  const auto trimmed=value::trim(input);const auto parsed=parseGitRemote(trimmed);
  if(parsed){
    if(parsed->host!="github.com")return std::nullopt;
    return parsed->owner+"/"+parsed->name;
  }
  if(trimmed.find("://")==std::string::npos&&trimmed.find('@')==std::string::npos&&
      trimmed.find('/')!=std::string::npos){
    const auto slash=trimmed.find('/');
    if(slash>0&&slash+1<trimmed.size()&&trimmed.find('/',slash+1)==std::string::npos){
      auto repo=trimmed.substr(slash+1);
      if(repo.ends_with(".git"))repo.resize(repo.size()-4);
      return trimmed.substr(0,slash)+"/"+repo;
    }
  }
  if(debug)debug("Could not parse repository from: "+trimmed);
  return std::nullopt;
}
}
