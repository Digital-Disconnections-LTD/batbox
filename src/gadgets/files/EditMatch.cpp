#include "gadgets/files/EditUtils.hpp"

namespace batbox::gadgets::files::edit {
namespace {
std::string replaceBytes(std::string text,const std::string& before,const std::string& after){
  std::size_t pos=0;
  while((pos=text.find(before,pos))!=std::string::npos){text.replace(pos,before.size(),after);pos+=after.size();}
  return text;
}
std::string trimEnd(const std::string& line){
  const auto trimmed=value::trim(line);
  if(trimmed.empty())return "";
  const auto start=line.find(trimmed);
  return line.substr(0,start+trimmed.size());
}
std::string replaceLiteral(const std::string& original,const std::string& oldText,
                           const std::string& newText,bool all){
  auto content=value::utf16CodeUnits(original);
  const auto oldUnits=value::utf16CodeUnits(oldText);
  const auto newUnits=value::utf16CodeUnits(newText);
  if(oldUnits.empty()){
    if(!all){content.insert(0,newUnits);return value::fromUtf16(content);}
    std::u16string result;result.reserve(content.size()+newUnits.size()*(content.size()+1));
    result+=newUnits;
    for(char16_t unit:content){result.push_back(unit);result+=newUnits;}
    return value::fromUtf16(result);
  }
  std::size_t pos=0;
  while((pos=content.find(oldUnits,pos))!=std::u16string::npos){
    content.replace(pos,oldUnits.size(),newUnits);
    pos+=newUnits.size();
    if(!all)break;
  }
  return value::fromUtf16(content);
}
} // namespace

std::string normalizeQuotes(std::string text){
  text=replaceBytes(std::move(text),"‘","'");
  text=replaceBytes(std::move(text),"’","'");
  text=replaceBytes(std::move(text),"“","\"");
  return replaceBytes(std::move(text),"”","\"");
}

std::string stripTrailingWhitespace(const std::string& text){
  std::string result;
  std::size_t start=0;
  while(start<text.size()){
    const auto ending=text.find_first_of("\r\n",start);
    if(ending==std::string::npos){result+=trimEnd(text.substr(start));return result;}
    result+=trimEnd(text.substr(start,ending-start));
    if(text[ending]=='\r'&&ending+1<text.size()&&text[ending+1]=='\n'){
      result+="\r\n";start=ending+2;
    }else{result+=text[ending];start=ending+1;}
  }
  return result;
}

std::optional<std::string> findActualString(const std::string& fileContent,const std::string& searchString){
  const auto contentUnits=value::utf16CodeUnits(fileContent);
  const auto searchUnits=value::utf16CodeUnits(searchString);
  if(contentUnits.find(searchUnits)!=std::u16string::npos)return searchString;
  const auto normalizedFile=value::utf16CodeUnits(normalizeQuotes(fileContent));
  const auto normalizedSearch=value::utf16CodeUnits(normalizeQuotes(searchString));
  const auto index=normalizedFile.find(normalizedSearch);
  if(index==std::u16string::npos)return std::nullopt;
  return value::substring(fileContent,index,index+searchUnits.size());
}

std::string applyEditToFile(const std::string& original,const std::string& oldString,
                            const std::string& newString,bool replaceAll){
  if(!newString.empty())return replaceLiteral(original,oldString,newString,replaceAll);
  const bool stripTrailingNewline=!oldString.ends_with('\n')&&
    value::utf16CodeUnits(original).find(value::utf16CodeUnits(oldString+"\n"))!=std::u16string::npos;
  return replaceLiteral(original,stripTrailingNewline?oldString+"\n":oldString,newString,replaceAll);
}

std::string convertLeadingTabsToSpaces(const std::string& content){
  if(content.find('\t')==std::string::npos)return content;
  std::string result;result.reserve(content.size());
  bool lineStart=true;
  for(char c:content){
    if(lineStart&&c=='\t'){result+="  ";continue;}
    result+=c;
    lineStart=(c=='\n'||c=='\r');
  }
  return result;
}
} // namespace batbox::gadgets::files::edit
