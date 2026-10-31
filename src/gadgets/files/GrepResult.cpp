#include "gadgets/files/Grep.hpp"

namespace batbox::gadgets::files {
namespace {
std::string join(const Json& values){
  std::string out;
  for(const auto& value:values){if(!out.empty())out+='\n';out+=value.get<std::string>();}
  return out;
}
std::optional<double> optionalNumber(const Json& value,const char* name){
  if(!value.contains(name)||value.at(name).is_null())return std::nullopt;
  return value.at(name).get<double>();
}
} // namespace

Json mapGrepResult(const Json& output,const std::string& id){
  const auto mode=output.value("mode",std::string("files_with_matches"));
  const auto info=formatLimitInfo(optionalNumber(output,"appliedLimit"),optionalNumber(output,"appliedOffset"));
  const std::string raw=output.value("content",std::string());
  std::string content;
  if(mode=="content"){
    content=raw.empty()?"No matches found":raw;
    if(!info.empty())content+="\n\n[Showing results with pagination = "+info+"]";
  }else if(mode=="count"){
    content=raw.empty()?"No matches found":raw;
    const double matches=output.value("numMatches",0.0);
    const double files=output.value("numFiles",0.0);
    const auto count=[](double n){return std::to_string(static_cast<long long>(n));};
    content+="\n\nFound "+count(matches)+" total "+(matches==1?"occurrence":"occurrences")+
      " across "+count(files)+" "+(files==1?"file":"files")+".";
    if(!info.empty())content+=" with pagination = "+info;
  }else{
    const double files=output.value("numFiles",0.0);
    if(files==0)content="No files found";
    else{
      content="Found "+std::to_string(static_cast<long long>(files))+" "+
        (files==1?"file":"files");
      if(!info.empty())content+=" "+info;
      content+='\n';content+=join(output.at("filenames"));
    }
  }
  return Json{{"tool_use_id",id},{"type","tool_result"},{"content",content}};
}
} // namespace batbox::gadgets::files
