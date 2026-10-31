#include "settings/schema/Runtime.hpp"
#include "settings/schema/Internal.hpp"
#include <cmath>
#include <stdexcept>

namespace batbox::settings::schema::detail {
namespace {
Json invalidTypeIssue(const std::string& expected,const Json& value,
  bool present,const Json& path){
  return {{"expected",expected},{"code","invalid_type"},{"path",path},
    {"message","Invalid input: expected "+expected+", received "+
      (present?typeName(value):"undefined")}};
}
std::string valueLabel(const Json& value){
  return value.is_string()?"\""+value.get<std::string>()+"\"":
    value::stringify(value);
}
}
ParseResult parseScalar(const Json& node,const Json& input,bool present,
  const Json& path,const MainConfig& config,const MainImports& imports){
  ParseResult result;result.present=present;result.value=input;
  const auto type=node.at("type").get<std::string>();
  if(type=="undefined"){
    result.valid=!present;
    if(!result.valid)result.issues.push_back(
      invalidTypeIssue("undefined",input,present,path));
    return result;
  }
  if(type=="string"||type=="number"||type=="boolean"){
    const bool matched=present&&(
      (type=="string"&&input.is_string())||
      (type=="number"&&input.is_number())||
      (type=="boolean"&&input.is_boolean()));
    if(!matched){result.issues.push_back(invalidTypeIssue(type,input,present,path));
      return result;}
    if(node.contains("checks"))for(const auto& check:node["checks"]){
      const auto kind=check.at("kind").get<std::string>();
      if(kind=="number_format"){
        const double number=input.get<double>();
        constexpr double limit=9007199254740991.0;
        if(std::isfinite(number)&&std::trunc(number)==number){
          if(number>limit)result.issues.push_back({{"code","too_big"},
            {"maximum",9007199254740991LL},
            {"note","Integers must be within the safe integer range."},
            {"origin","int"},{"path",path},
            {"message","Too big: expected int to be <9007199254740991"}});
          else if(number< -limit)result.issues.push_back({{"code","too_small"},
            {"minimum",-9007199254740991LL},
            {"note","Integers must be within the safe integer range."},
            {"origin","int"},{"path",path},
            {"message","Too small: expected int to be >-9007199254740991"}});
          else continue;
        }else result.issues.push_back({{"expected","int"},
          {"format","safeint"},{"code","invalid_type"},{"path",path},
          {"message","Invalid input: expected int, received number"}});
      }else if(kind=="greater_than"){
        const double number=input.get<double>();
        const double minimum=check.at("value").get<double>();
        const bool inclusive=check.value("inclusive",false);
        if(inclusive?number>=minimum:number>minimum)continue;
        const auto limit=value::numberToString(minimum);
        result.issues.push_back({{"origin","number"},{"code","too_small"},
          {"minimum",check.at("value")},{"inclusive",inclusive},
          {"path",path},{"message",std::string("Too small: expected number to be ")+
          (inclusive?">=":">")+limit}});
      }else if(kind=="string_format"){
        if(check.value("format",std::string())!="url")
          throw std::runtime_error("Unbound settings string format");
        if(!imports.canonicalUrl)
          throw std::runtime_error("Missing settings import: canonicalUrl");
        const auto original=input.get<std::string>();
        const auto href=imports.canonicalUrl(original);
        if(!href)
          result.issues.push_back({{"code","invalid_format"},{"format","url"},
            {"path",path},{"message","Invalid URL"}});
        else result.value=!original.ends_with('/')&&href->ends_with('/')
          ?href->substr(0,href->size()-1):*href;
      }else throw std::runtime_error("Unbound settings scalar check: "+kind);
    }
    result.valid=result.issues.empty();return result;
  }
  if(type=="enum"||type=="literal"){
    const Json& values=node.at("values");
    for(const auto& value:values){
      if(present&&(value==input||
        (type=="literal"&&value=="batbox://settings-schema"&&
          input==config.schemaUrl))){result.valid=true;return result;}
    }
    std::string names;
    for(const auto& value:values){
      if(!names.empty())names+='|';
      names+=valueLabel(value=="batbox://settings-schema"&&type=="literal"
        ?Json(config.schemaUrl):value);
    }
    Json configured=values;
    if(type=="literal"&&values.size()==1&&
        values[0]=="batbox://settings-schema")
      configured=Json::array({config.schemaUrl});
    result.issues.push_back({{"code","invalid_value"},{"values",configured},
      {"path",path},{"message",values.size()==1?
        "Invalid input: expected "+names:
        "Invalid option: expected one of "+names}});
    return result;
  }
  throw std::runtime_error("Unbound settings scalar type: "+type);
}
}
