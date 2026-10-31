#include "settings/mdm/Parse.hpp"
#include "runtime/JsonValue.hpp"
#include <cctype>
#include <filesystem>
#include <stdexcept>

namespace batbox::settings::mdm {
namespace {
bool present(const Json& value){
  return (value.is_object()||value.is_array())&&!value.empty();
}
std::size_t utf8Width(unsigned char first){
  if(first<0x80)return 1;
  if((first&0xe0)==0xc0)return 2;
  if((first&0xf0)==0xe0)return 3;
  if((first&0xf8)==0xf0)return 4;
  return 1;
}
bool takeSpace(const std::string& line,std::size_t& position){
  const auto before=position;
  while(position<line.size()){
    const auto width=std::min(utf8Width(
      static_cast<unsigned char>(line[position])),line.size()-position);
    if(!value::trim(std::string_view(line).substr(position,width)).empty())break;
    position+=width;
  }
  return position>before;
}
bool literalI(const std::string& text,std::size_t& position,
    const std::string& expected){
  if(text.size()-position<expected.size())return false;
  for(std::size_t i=0;i<expected.size();++i){
    const auto lhs=static_cast<unsigned char>(text[position+i]);
    const auto rhs=static_cast<unsigned char>(expected[i]);
    if(std::tolower(lhs)!=std::tolower(rhs))return false;
  }
  position+=expected.size();return true;
}
}
MdmResult parseCommandOutputAsSettings(const std::string& stdoutText,
    const std::string& sourcePath,const ParseServices& services){
  if(!services.safeJson||!services.parser.filterInvalidPermissionRules||
      !services.parser.validateSettingsSchema||
      !services.parser.formatZodError)
    throw std::invalid_argument("Missing MDM parser binding");
  Json data=services.safeJson->safeParseJSON(stdoutText,false);
  if(!value::truthy(data)||!(data.is_object()||data.is_array()))
    return {Json::object(),Json::array()};
  auto warnings=services.parser.filterInvalidPermissionRules(data,sourcePath);
  const auto result=services.parser.validateSettingsSchema(data);
  if(!result.success){
    auto errors=warnings;
    const auto formatted=services.parser.formatZodError(
      result.issues,sourcePath);
    for(const auto& error:formatted)errors.push_back(error);
    return {Json::object(),errors};
  }
  return {result.data,warnings};
}
std::optional<std::string> parseRegQueryStdout(
    const std::string& stdoutText,const std::string& valueName){
  std::size_t start=0;
  while(start<=stdoutText.size()){
    const auto end=stdoutText.find('\n',start);
    const auto stop=end==std::string::npos?stdoutText.size():end;
    auto line=stdoutText.substr(start,stop-start);
    if(!line.empty()&&line.back()=='\r'&&end!=std::string::npos)
      line.pop_back();
    std::size_t position=0;
    if(takeSpace(line,position)&&literalI(line,position,valueName)&&
        takeSpace(line,position)&&literalI(line,position,"REG_")&&
        (literalI(line,position,"EXPAND_SZ")||
          literalI(line,position,"SZ"))&&takeSpace(line,position)&&
        position<line.size())
      return value::trim(line.substr(position));
    if(end==std::string::npos)break;
    start=end+1;
  }
  return std::nullopt;
}
bool hasManagedSettingsFile(const ParseServices& services){
  if(!services.paths||!services.safeJson||!services.readFileSync||
      !services.readdirSync)
    throw std::invalid_argument("Missing managed MDM file binding");
  try{
    const auto file=services.paths->getManagedSettingsFilePath();
    const auto content=services.readFileSync(file);
    const auto data=services.safeJson->safeParseJSON(content,false);
    if(present(data))return true;
  }catch(...){}
  try{
    const auto directory=services.paths->getManagedSettingsDropInDir();
    const auto entries=services.readdirSync(directory);
    for(const auto& entry:entries){
      if(!(entry.file||entry.symlink)||
          !entry.name.ends_with(".json")||entry.name.starts_with('.'))
        continue;
      try{
        const auto file=(std::filesystem::path(directory)/entry.name).string();
        const auto content=services.readFileSync(file);
        const auto data=services.safeJson->safeParseJSON(content,false);
        if(present(data))return true;
      }catch(...){}
    }
  }catch(...){}
  return false;
}
ParsedRaw consumeRawReadResult(const RawReadResult& raw,
    const ParseServices& services){
  const MdmResult empty{Json::object(),Json::array()};
  if(raw.plistStdouts&&!raw.plistStdouts->empty()){
    const auto& first=raw.plistStdouts->front();
    auto result=parseCommandOutputAsSettings(first.stdoutText,
      first.label,services);
    if(present(result.settings))return {std::move(result),empty};
  }
  if(raw.hklmStdout&&!raw.hklmStdout->empty()){
    const auto parsed=parseRegQueryStdout(*raw.hklmStdout);
    if(parsed){
      auto result=parseCommandOutputAsSettings(*parsed,
        "Registry: "+services.names.windowsHKLM+"\\"+
          services.names.windowsValue,services);
      if(present(result.settings))return {std::move(result),empty};
    }
  }
  if(hasManagedSettingsFile(services))return {empty,empty};
  if(raw.hkcuStdout&&!raw.hkcuStdout->empty()){
    const auto parsed=parseRegQueryStdout(*raw.hkcuStdout);
    if(parsed){
      auto result=parseCommandOutputAsSettings(*parsed,
        "Registry: "+services.names.windowsHKCU+"\\"+
          services.names.windowsValue,services);
      return {empty,std::move(result)};
    }
  }
  return {empty,empty};
}
}
