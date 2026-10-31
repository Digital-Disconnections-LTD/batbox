#include "termio/parser/StyleCompare.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <limits>
#include <string>
using batbox::Json;
using namespace batbox::termio::parser;
namespace {
double numeric(const Json& value){
  if(value.is_string()&&value.get<std::string>()=="NaN")
    return std::numeric_limits<double>::quiet_NaN();
  return value.get<double>();
}
Color color(const Json& value){
  Color result;result.type=value.at("type").get<std::string>();
  if(result.type=="named")result.name=value.at("name").get<std::string>();
  else if(result.type=="indexed")result.index=numeric(value.at("index"));
  else if(result.type=="rgb"){
    result.r=numeric(value.at("r"));result.g=numeric(value.at("g"));
    result.b=numeric(value.at("b"));
  }
  return result;
}
TextStyle style(const Json& value){
  auto result=defaultStyle();
  for(auto key:{"bold","dim","italic","blink","inverse","hidden",
                "strikethrough","overline"}){
    if(!value.contains(key))continue;
    const auto enabled=value.at(key).get<bool>();
    if(std::string(key)=="bold")result.bold=enabled;
    else if(std::string(key)=="dim")result.dim=enabled;
    else if(std::string(key)=="italic")result.italic=enabled;
    else if(std::string(key)=="blink")result.blink=enabled;
    else if(std::string(key)=="inverse")result.inverse=enabled;
    else if(std::string(key)=="hidden")result.hidden=enabled;
    else if(std::string(key)=="strikethrough")result.strikethrough=enabled;
    else result.overline=enabled;
  }
  if(value.contains("underline"))result.underline=value.at("underline").get<std::string>();
  if(value.contains("fg"))result.fg=color(value.at("fg"));
  if(value.contains("bg"))result.bg=color(value.at("bg"));
  if(value.contains("underlineColor"))result.underlineColor=color(value.at("underlineColor"));
  return result;
}
}
int main(int argc,char** argv){
  if(argc!=2||std::string(argv[1])!="--probe")return 2;
  std::string line;
  while(std::getline(std::cin,line)){
    try{
      const auto input=batbox::value::parse(line);
      const auto kind=input.at("kind").get<std::string>();
      Json result;
      if(kind=="default")result=toJson(defaultStyle());
      else if(kind=="color")result=colorsEqual(color(input.at("a")),color(input.at("b")));
      else if(kind=="style")result=stylesEqual(style(input.at("a")),style(input.at("b")));
      else throw std::runtime_error("unknown kind");
      std::cout<<batbox::value::stringify(Json{{"result",result}})<<'\n';
    }catch(const std::exception& error){
      std::cout<<batbox::value::stringify(Json{{"error",error.what()}})<<'\n';
    }
  }
}
