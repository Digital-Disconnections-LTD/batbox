#include "interactive/message_list_view/Search.hpp"
#include <unicode/locid.h>
#include <unicode/unistr.h>
#include <algorithm>

namespace batbox::interactive::message_list_view {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
std::string join(const std::vector<std::string>& parts,const char* delimiter){
  std::string result;
  for(std::size_t i=0;i<parts.size();++i){if(i)result+=delimiter;result+=parts[i];}
  return result;
}
void appendArrays(const Json& value,std::vector<std::string>& parts,
    std::initializer_list<const char*> keys,const char* delimiter){
  for(const auto* key:keys){
    const auto& array=field(value,key);
    if(!array.is_array()||!std::all_of(array.begin(),array.end(),
        [](const Json& item){return item.is_string();}))continue;
    parts.push_back(join(array.get<std::vector<std::string>>(),delimiter));
  }
}
}
std::string lowercase(const std::string& text){
  const auto utf16=value::utf16CodeUnits(text);
  icu::UnicodeString converted(reinterpret_cast<const UChar*>(utf16.data()),
    static_cast<int32_t>(utf16.size()));
  converted.toLower(icu::Locale::getRoot());
  return value::fromUtf16(std::u16string_view(
    reinterpret_cast<const char16_t*>(converted.getBuffer()),converted.length()));
}
std::string gadgetUseSearchText(const Json& input){
  if(!input.is_object()&&!input.is_array())return {};
  std::vector<std::string> parts;
  for(const auto* key:{"command","pattern","file_path","path","prompt",
      "description","query","url","skill"}){
    const auto& value=field(input,key);
    if(value.is_string())parts.push_back(value.get<std::string>());
  }
  appendArrays(input,parts,{"args","files"}," ");
  return join(parts,"\n");
}
std::string gadgetResultSearchText(const Json& result){
  if(!result.is_object()&&!result.is_array())
    return result.is_string()?result.get<std::string>():std::string{};
  const auto& stdoutText=field(result,"stdout");
  if(stdoutText.is_string()){
    const auto& stderrText=field(result,"stderr");
    const auto error=stderrText.is_string()?stderrText.get<std::string>():std::string{};
    return stdoutText.get<std::string>()+(error.empty()?"":"\n"+error);
  }
  const auto& file=field(result,"file");
  if((file.is_object()||file.is_array())&&field(file,"content").is_string())
    return field(file,"content").get<std::string>();
  std::vector<std::string> parts;
  for(const auto* key:{"content","output","result","text","message"}){
    const auto& value=field(result,key);
    if(value.is_string())parts.push_back(value.get<std::string>());
  }
  appendArrays(result,parts,{"filenames","lines","results"},"\n");
  return join(parts,"\n");
}
} // namespace batbox::interactive::message_list_view
