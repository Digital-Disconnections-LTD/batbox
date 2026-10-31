#include "gadgets/files/EditUtils.hpp"
#include <algorithm>
#include <stdexcept>
#include <vector>

namespace batbox::gadgets::files::edit {
namespace {
const std::vector<std::pair<std::string,std::string>> desanitizations={
  {"<fnr>","<function_results>"},{"<n>","<name>"},{"</n>","</name>"},
  {"<o>","<output>"},{"</o>","</output>"},{"<e>","<error>"},{"</e>","</error>"},
  {"<s>","<system>"},{"</s>","</system>"},{"<r>","<result>"},{"</r>","</result>"},
  {"< META_START >","<META_START>"},{"< META_END >","<META_END>"},
  {"< EOT >","<EOT>"},{"< META >","<META>"},{"< SOS >","<SOS>"},
  {"\n\nH:","\n\nHuman:"},{"\n\nA:","\n\nAssistant:"}
};
std::string replaceAll(std::string value,const std::string& from,const std::string& to){
  std::size_t pos=0;
  while((pos=value.find(from,pos))!=std::string::npos){value.replace(pos,from.size(),to);pos+=to.size();}
  return value;
}
bool contains(const std::string& value,const std::string& part){
  return value::utf16CodeUnits(value).find(value::utf16CodeUnits(part))!=std::u16string::npos;
}
struct Desanitized{std::string result;std::vector<std::pair<std::string,std::string>> applied;};
Desanitized desanitizeMatchString(const std::string& text){
  Desanitized out{text,{}};
  for(const auto& pair:desanitizations){
    const auto before=out.result;
    out.result=replaceAll(out.result,pair.first,pair.second);
    if(before!=out.result)out.applied.push_back(pair);
  }
  return out;
}
bool isMarkdown(std::string path){
  std::transform(path.begin(),path.end(),path.begin(),
    [](unsigned char c){return static_cast<char>(std::tolower(c));});
  return path.ends_with(".md")||path.ends_with(".mdx");
}
} // namespace

Json normalizeFileEditInput(const Json& input,const Services& services){
  const auto path=input.at("file_path").get<std::string>();
  const auto& edits=input.at("edits");
  if(edits.empty())return input;
  const bool markdown=isMarkdown(path);
  if(!services.readFileSyncStashed)
    throw std::invalid_argument("Missing FileEdit binding: readFileSyncCached");
  try{
    const auto full=filesystem::expandPath(path,services.paths);
    const auto file=services.readFileSyncStashed(full);
    Json result=Json::array();
    for(const auto& edit:edits){
      const auto oldString=edit.at("old_string").get<std::string>();
      const auto newString=edit.at("new_string").get<std::string>();
      std::string normalizedNew=markdown?newString:stripTrailingWhitespace(newString);
      std::string actualOld=oldString;
      if(!contains(file,oldString)){
        const auto desanitized=desanitizeMatchString(oldString);
        if(contains(file,desanitized.result)){
          actualOld=desanitized.result;
          for(const auto& pair:desanitized.applied)
            normalizedNew=replaceAll(normalizedNew,pair.first,pair.second);
        }
      }
      Json normalized={{"old_string",actualOld},{"new_string",normalizedNew}};
      if(edit.contains("replace_all"))normalized["replace_all"]=edit.at("replace_all");
      result.push_back(normalized);
    }
    return Json{{"file_path",path},{"edits",result}};
  }catch(const std::exception&){return input;}
}
} // namespace batbox::gadgets::files::edit
