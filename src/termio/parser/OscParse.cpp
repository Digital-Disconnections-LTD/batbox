#include "termio/parser/Parser.hpp"
#include <cmath>
#include <cctype>
#include <utility>

namespace batbox::termio::parser {
namespace {
std::vector<std::string> split(const std::string& text,char separator){
  std::vector<std::string> result;std::size_t start=0;
  while(start<=text.size()){
    const auto end=text.find(separator,start);
    if(end==std::string::npos){result.push_back(text.substr(start));break;}
    result.push_back(text.substr(start,end-start));start=end+1;
  }
  return result;
}
bool hex(char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F');}
double hexNumber(const std::string& value){return static_cast<double>(std::stoul(value,nullptr,16));}
std::vector<std::pair<std::string,std::string>> splitTabStatusPairs(const std::string& data){
  std::vector<std::pair<std::string,std::string>> result;
  std::string key,val;bool inVal=false,esc=false;
  for(const auto c:data){
    if(esc){(inVal?val:key)+=c;esc=false;}
    else if(c=='\\')esc=true;
    else if(c==';'){
      result.emplace_back(key,val);key.clear();val.clear();inVal=false;
    }else if(c=='='&&!inVal)inVal=true;
    else (inVal?val:key)+=c;
  }
  if(!key.empty()||inVal)result.emplace_back(key,val);
  return result;
}
TabStatusAction parseTabStatus(const std::string& data){
  TabStatusAction result;
  for(const auto& [key,value]:splitTabStatusPairs(data)){
    if(key=="indicator")result.indicator=value.empty()?
      std::optional<Color>{}:parseOscColor(value);
    else if(key=="status")result.status=value.empty()?
      std::optional<std::string>{}:std::optional<std::string>(value);
    else if(key=="status-color")result.statusColor=value.empty()?
      std::optional<Color>{}:parseOscColor(value);
  }
  return result;
}
}
std::optional<Color> parseOscColor(const std::string& spec){
  if(spec.size()==7&&spec[0]=='#'){
    for(std::size_t i=1;i<spec.size();++i)if(!hex(spec[i]))return std::nullopt;
    Color result;result.type="rgb";
    result.r=hexNumber(spec.substr(1,2));result.g=hexNumber(spec.substr(3,2));
    result.b=hexNumber(spec.substr(5,2));return result;
  }
  if(spec.size()>=4&&
     std::tolower(static_cast<unsigned char>(spec[0]))=='r'&&
     std::tolower(static_cast<unsigned char>(spec[1]))=='g'&&
     std::tolower(static_cast<unsigned char>(spec[2]))=='b'&&spec[3]==':'){
    const auto components=split(spec.substr(4),'/');
    if(components.size()!=3)return std::nullopt;
    for(const auto& component:components){
      if(component.empty()||component.size()>4)return std::nullopt;
      for(const auto c:component)if(!hex(c))return std::nullopt;
    }
    auto scale=[](const std::string& component){
      return std::floor(hexNumber(component)/(std::pow(16,component.size())-1)*255+0.5);
    };
    Color result;result.type="rgb";
    result.r=scale(components[0]);result.g=scale(components[1]);result.b=scale(components[2]);
    return result;
  }
  return std::nullopt;
}
std::optional<Action> parseOSC(const std::string& content){
  const auto semi=content.find(';');
  const auto command=semi==std::string::npos?content:content.substr(0,semi);
  const auto data=semi==std::string::npos?std::string{}:content.substr(semi+1);
  const auto commandNum=value::parseInt10(command);
  if(commandNum==0)return TitleAction{"both",data};
  if(commandNum==1)return TitleAction{"iconName",data};
  if(commandNum==2)return TitleAction{"windowTitle",data};
  if(commandNum==8){
    const auto parts=split(data,';');
    const auto paramsStr=parts.empty()?std::string{}:parts[0];
    std::string url;
    for(std::size_t i=1;i<parts.size();++i){if(i>1)url+=';';url+=parts[i];}
    if(url.empty()){
      LinkAction result;result.type="end";return result;
    }
    Json params=Json::object();
    if(!paramsStr.empty())for(const auto& pair:split(paramsStr,':')){
      const auto equal=pair.find('=');
      if(equal!=std::string::npos){
        const auto key=pair.substr(0,equal);
        if(key!="__proto__")params[key]=pair.substr(equal+1);
      }
    }
    LinkAction result;result.type="start";result.url=url;
    if(!params.empty())result.params=std::move(params);
    return result;
  }
  if(commandNum==21337)return parseTabStatus(data);
  return UnknownAction{"\x1b]"+content};
}
}
