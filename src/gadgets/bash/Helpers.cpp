#include "gadgets/bash/Helpers.hpp"
#include <algorithm>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace batbox::bash_gadget {
std::string stripEmptyLines(const std::string& content){
  std::vector<std::string> lines;std::size_t begin=0;
  for(;;){const auto end=content.find('\n',begin);
    lines.push_back(content.substr(begin,end==std::string::npos?std::string::npos:end-begin));
    if(end==std::string::npos)break;
    begin=end+1;
  }
  std::size_t start=0;while(start<lines.size()&&value::trim(lines[start]).empty())++start;
  std::size_t end=lines.size();while(end>start&&value::trim(lines[end-1]).empty())--end;
  if(start>=end)return {};
  std::string result=lines[start];
  for(std::size_t i=start+1;i<end;i++)result+='\n'+lines[i];
  return result;
}
bool isImageOutput(const std::string& content){
  static const std::regex image(R"(^data:image/[a-z0-9.+_-]+;base64,)",std::regex::icase);
  return std::regex_search(content,image);
}
std::optional<Json> parseDataUri(const std::string& content){
  static const std::regex uri(R"(^data:([^;]+);base64,(.+)$)");
  std::smatch match;const auto trimmed=value::trim(content);
  if(!std::regex_match(trimmed,match,uri)||match[1].str().empty()||match[2].str().empty())return std::nullopt;
  return Json{{"mediaType",match[1].str()},{"data",match[2].str()}};
}
Json buildImageGadgetResult(const std::string& stdoutText,const std::string& gadgetUseID){
  const auto parsed=parseDataUri(stdoutText);if(!parsed)return nullptr;
  return {{"tool_use_id",gadgetUseID},{"type","tool_result"},{"content",Json::array({
    {{"type","image"},{"source",{{"type","base64"},{"media_type",parsed->at("mediaType")},{"data",parsed->at("data")}}}}
  })}};
}
Json formatOutput(const std::string& content,std::size_t maxOutputLength){
  const bool image=isImageOutput(content);
  if(image)return {{"totalLines",1},{"truncatedContent",content},{"isImage",true}};
  const auto totalLines=static_cast<long>(std::count(content.begin(),content.end(),'\n'))+1;
  if(value::stringLength(content)<=maxOutputLength)
    return {{"totalLines",totalLines},{"truncatedContent",content},{"isImage",false}};
  const auto prefix=value::slice(content,0,maxOutputLength);
  const auto suffix=value::slice(content,maxOutputLength);
  const auto remainingLines=static_cast<long>(std::count(suffix.begin(),suffix.end(),'\n'))+1;
  return {{"totalLines",totalLines},{"truncatedContent",prefix+"\n\n... ["+
    std::to_string(remainingLines)+" lines truncated] ..."},{"isImage",false}};
}
Json interpretCommandResult(const std::string& command,int exitCode,
  const std::string&,const std::string&,const SplitCommand& splitCommand){
  if(!splitCommand)throw std::logic_error("splitCommand dependency not supplied");
  const auto segments=splitCommand(command);
  const auto last=segments.empty()?command:segments.back();
  const auto trimmed=value::trim(last);
  const auto whitespace=trimmed.find_first_of(" \t\r\n");
  const auto base=trimmed.substr(0,whitespace);
  const bool known=base=="grep"||base=="rg"||base=="find"||base=="diff"||base=="test"||base=="[";
  if(known){
    if(exitCode==1){
      const auto message=base=="grep"||base=="rg"?"No matches found":
        base=="find"?"Some directories were inaccessible":base=="diff"?"Files differ":"Condition is false";
      return {{"isError",false},{"message",message}};
    }
    return {{"isError",exitCode>=2}};
  }
  if(exitCode!=0)return {{"isError",true},{"message","Command failed with exit code "+std::to_string(exitCode)}};
  return {{"isError",false}};
}
std::optional<std::string> detectBlockedSleepPattern(const std::string& command,const SplitCommand& splitCommand){
  if(!splitCommand)throw std::logic_error("splitCommand dependency not supplied");
  const auto parts=splitCommand(command);if(parts.empty())return std::nullopt;
  const auto first=value::utf16CodeUnits(value::trim(parts[0]));
  if(first.size()<7||first.substr(0,5)!=u"sleep")return std::nullopt;
  auto white=[](char16_t unit){
    const auto text=value::fromUtf16(std::u16string_view(&unit,1));
    return value::trim(text).empty();
  };
  std::size_t at=5;if(!white(first[at]))return std::nullopt;
  while(at<first.size()&&white(first[at]))++at;
  const auto digitsAt=at;
  while(at<first.size()&&first[at]>=u'0'&&first[at]<=u'9')++at;
  if(at==digitsAt)return std::nullopt;
  const auto digits=value::fromUtf16(std::u16string_view(first.data()+digitsAt,at-digitsAt));
  while(at<first.size()&&white(first[at]))++at;
  if(at!=first.size())return std::nullopt;
  const auto secs=value::parseInt10(digits);if(secs<2)return std::nullopt;
  std::string rest;
  for(std::size_t i=1;i<parts.size();++i){if(i>1)rest+=' ';rest+=parts[i];}
  rest=value::trim(rest);
  const auto count=value::numberToString(secs);
  return rest.empty()?"standalone sleep "+count:"sleep "+count+" followed by: "+rest;
}
namespace {
const std::unordered_set<std::string> neutral={"echo","printf","true","false",":"};
const std::unordered_set<std::string> search={"find","grep","rg","ag","ack","locate","which","whereis"};
const std::unordered_set<std::string> read={"cat","head","tail","less","more","wc","stat","file","strings","jq","awk","cut","sort","uniq","tr"};
const std::unordered_set<std::string> list={"ls","tree","du"};
const std::unordered_set<std::string> silent={"mv","cp","rm","mkdir","rmdir","chmod","chown","chgrp","touch","ln","cd","export","unset","wait"};
std::string baseCommand(const std::string& part){
  const auto trimmed=value::utf16CodeUnits(value::trim(part));std::size_t end=0;
  while(end<trimmed.size()){
    const auto text=value::fromUtf16(std::u16string_view(&trimmed[end],1));
    if(value::trim(text).empty())break;
    ++end;
  }
  return value::fromUtf16(std::u16string_view(trimmed.data(),end));
}
bool isOperator(const std::string& part){return part=="||"||part=="&&"||part=="|"||part==";";}
bool isRedirect(const std::string& part){return part==">"||part==">>"||part==">&";}
}
Json isSearchOrReadBashCommand(const std::string& command,const SplitCommand& splitWithOperators){
  if(!splitWithOperators)throw std::logic_error("splitWithOperators dependency not supplied");
  auto result=Json{{"isSearch",false},{"isRead",false},{"isList",false}};
  std::vector<std::string> parts;
  try{parts=splitWithOperators(command);}catch(...){return result;}
  if(parts.empty())return result;
  bool hasSearch=false,hasRead=false,hasList=false,hasNonNeutral=false,skipRedirectTarget=false;
  for(const auto& part:parts){
    if(skipRedirectTarget){skipRedirectTarget=false;continue;}
    if(isRedirect(part)){skipRedirectTarget=true;continue;}
    if(isOperator(part))continue;
    const auto base=baseCommand(part);if(base.empty())continue;
    if(neutral.contains(base))continue;
    hasNonNeutral=true;
    const bool partSearch=search.contains(base),partRead=read.contains(base),partList=list.contains(base);
    if(!partSearch&&!partRead&&!partList)return result;
    if(partSearch)hasSearch=true;
    if(partRead)hasRead=true;
    if(partList)hasList=true;
  }
  if(!hasNonNeutral)return result;
  result["isSearch"]=hasSearch;result["isRead"]=hasRead;result["isList"]=hasList;
  return result;
}
bool isSilentBashCommand(const std::string& command,const SplitCommand& splitWithOperators){
  if(!splitWithOperators)throw std::logic_error("splitWithOperators dependency not supplied");
  std::vector<std::string> parts;
  try{parts=splitWithOperators(command);}catch(...){return false;}
  if(parts.empty())return false;
  bool hasNonFallback=false,skipRedirectTarget=false;std::string lastOperator;
  for(const auto& part:parts){
    if(skipRedirectTarget){skipRedirectTarget=false;continue;}
    if(isRedirect(part)){skipRedirectTarget=true;continue;}
    if(isOperator(part)){lastOperator=part;continue;}
    const auto base=baseCommand(part);if(base.empty())continue;
    if(lastOperator=="||"&&neutral.contains(base))continue;
    hasNonFallback=true;
    if(!silent.contains(base))return false;
  }
  return hasNonFallback;
}
} // namespace batbox::bash_gadget
