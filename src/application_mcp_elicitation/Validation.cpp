#include "application_mcp_elicitation/Validation.hpp"
#include "runtime/Url.hpp"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <regex>

namespace batbox::application_mcp_elicitation {
namespace {
std::string field(const Json& value,const char* name){
  const auto& found=value::property(value,name);
  return found.is_string()?found.get<std::string>():std::string{};
}
std::string label(const Json& value){
  return value.is_string()?value.get<std::string>():value.dump();
}
void append(const Json& values,const Json& titles,std::vector<Option>& output){
  if(!values.is_array())return;
  for(std::size_t index=0;index<values.size();++index){
    const auto& value=values[index];
    const auto& title=titles.is_array()&&index<titles.size()?titles[index]:value;
    output.push_back({value,label(title)});
  }
}
bool validDate(const std::string& value){
  if(value.size()!=10||value[4]!='-'||value[7]!='-')return false;
  try{
    const auto year=std::stoi(value.substr(0,4));
    const auto month=std::stoi(value.substr(5,2));
    const auto day=std::stoi(value.substr(8,2));
    for(auto index:{0,1,2,3,5,6,8,9})if(value[index]<'0'||value[index]>'9')return false;
    return std::chrono::year_month_day(std::chrono::year(year)/
      std::chrono::month(static_cast<unsigned>(month))/
      std::chrono::day(static_cast<unsigned>(day))).ok();
  }catch(...){return false;}
}
bool validDateTime(const std::string& value){
  static const std::regex pattern(
    R"(^(\d{4}-\d{2}-\d{2})T(\d{2}):(\d{2}):(\d{2})(?:\.\d+)?(Z|[+-]\d{2}:\d{2})$)");
  std::smatch match;
  if(!std::regex_match(value,match,pattern)||!validDate(match[1]))return false;
  const int hour=std::stoi(match[2]),minute=std::stoi(match[3]),
    second=std::stoi(match[4]);
  if(hour>23||minute>59||second>59)return false;
  const auto zone=match[5].str();
  return zone=="Z"||(std::stoi(zone.substr(1,2))<=23&&
    std::stoi(zone.substr(4,2))<=59);
}
std::optional<double> numberValue(const std::string& raw){
  const auto trimmed=value::trim(raw);
  if(trimmed.empty())return 0.0; // Number('') / z.coerce.number('')
  if(trimmed.size()>2&&trimmed[0]=='0'&&
      (trimmed[1]=='x'||trimmed[1]=='X'||trimmed[1]=='b'||
       trimmed[1]=='B'||trimmed[1]=='o'||trimmed[1]=='O')){
    const int base=(trimmed[1]=='x'||trimmed[1]=='X')?16:
      (trimmed[1]=='b'||trimmed[1]=='B')?2:8;
    try{std::size_t used=0;const auto number=std::stoull(trimmed.substr(2),&used,base);
      if(used==trimmed.size()-2)return static_cast<double>(number);
    }catch(...){}return {};
  }
  char* end=nullptr;
  const double value=std::strtod(trimmed.c_str(),&end);
  if(!end||end==trimmed.c_str()||*end!='\0'||!std::isfinite(value))return {};
  return value;
}
std::string rangeMessage(const Json& schema,bool integer){
  const auto& minimum=value::property(schema,"minimum");
  const auto& maximum=value::property(schema,"maximum");
  const std::string base=integer?"Must be an integer":"Must be a number";
  if(minimum.is_number()&&maximum.is_number())return base+" between "+
    minimum.dump()+" and "+maximum.dump();
  if(minimum.is_number())return base+" >= "+minimum.dump();
  if(maximum.is_number())return base+" <= "+maximum.dump();
  return base;
}
}
std::vector<Option> optionsFor(const Json& schema){
  std::vector<Option> output;
  const auto& items=field(schema,"type")=="array"?
    value::property(schema,"items"):schema;
  const auto& oneOf=value::property(items,field(schema,"type")=="array"?"anyOf":"oneOf");
  if(oneOf.is_array()){
    for(const auto& item:oneOf)if(item.is_object()&&item.contains("const"))
      output.push_back({item.at("const"),field(item,"title").empty()?
        label(item.at("const")):field(item,"title")});
    return output;
  }
  append(value::property(items,"enum"),value::property(items,"enumNames"),output);
  return output;
}
std::optional<Json> validateInput(const std::string& raw,const Json& schema,
    bool required,std::string& error){
  const auto type=field(schema,"type");
  if(raw.empty()&&!required)return Json();
  if(raw.empty()&&required){error="This field is required.";return {};}
  if(type=="string"){
    const auto options=optionsFor(schema);
    if(!options.empty()){
      for(const auto& option:options)if(option.value==raw)return Json(raw);
      error="Choose one of the listed values.";return {};
    }
    const auto& minimum=value::property(schema,"minLength");
    const auto& maximum=value::property(schema,"maxLength");
    const auto length=value::stringLength(raw);
    if(minimum.is_number_integer()&&minimum.get<std::int64_t>()>=0&&
        length<static_cast<std::size_t>(minimum.get<std::int64_t>())){
      const auto count=static_cast<std::size_t>(minimum.get<std::int64_t>());
      error="Must be at least "+std::to_string(count)+
        (count==1?" character":" characters");return {};
    }
    if(maximum.is_number_integer()&&maximum.get<std::int64_t>()>=0&&
        length>static_cast<std::size_t>(maximum.get<std::int64_t>())){
      const auto count=static_cast<std::size_t>(maximum.get<std::int64_t>());
      error="Must be at most "+std::to_string(count)+
        (count==1?" character":" characters");return {};
    }
    const auto format=field(schema,"format");
    if(format=="email"){
      static const std::regex email(R"(^[^\s@]+@[^\s@]+\.[^\s@]+$)");
      if(!std::regex_match(raw,email)){
        error="Must be a valid email address, e.g. user@example.com";return {};
      }
    }else if(format=="uri"&&!runtime::canParseUrl(raw)){
      error="Must be a valid URI, e.g. https://example.com";return {};
    }else if(format=="date"&&!validDate(raw)){
      error="Must be a valid date, e.g. 2024-03-15, today, next Monday";return {};
    }else if(format=="date-time"&&!validDateTime(raw)){
      error="Must be a valid date-time, e.g. 2024-03-15T14:30:00Z, tomorrow at 3pm";
      return {};
    }
    return Json(raw);
  }
  if(type=="integer"||type=="number"){
    const bool integer=type=="integer";
    const auto number=numberValue(raw);
    const auto& minimum=value::property(schema,"minimum");
    const auto& maximum=value::property(schema,"maximum");
    if(!number||(integer&&(!std::isfinite(*number)||std::trunc(*number)!=*number||
        std::abs(*number)>9007199254740991.0))||
        (minimum.is_number()&&*number<minimum.get<double>())||
        (maximum.is_number()&&*number>maximum.get<double>())){
      error=rangeMessage(schema,integer);return {};
    }
    if(integer)return Json(static_cast<std::int64_t>(*number));
    return Json(*number);
  }
  error="This field type is not supported by the native form.";return {};
}
}
