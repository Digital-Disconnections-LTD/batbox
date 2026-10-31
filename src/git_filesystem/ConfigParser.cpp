#include "git_filesystem/ConfigParser.hpp"
#include "git_filesystem/GitFilesystemInternal.hpp"
#include <unicode/locid.h>
#include <unicode/unistr.h>

namespace batbox::git_filesystem {
namespace {
bool keyChar(char ch){return (ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||
  (ch>='0'&&ch<='9')||ch=='-';}
std::string lower(std::string value){
  const auto units=value::utf16CodeUnits(value);
  icu::UnicodeString text(reinterpret_cast<const UChar*>(units.data()),
    static_cast<int32_t>(units.size()));
  text.toLower(icu::Locale::getRoot());
  return value::fromUtf16(std::u16string_view(
    reinterpret_cast<const char16_t*>(text.getBuffer()),text.length()));
}
std::string trimTrailingWhitespace(const std::string& value){
  std::size_t end=value.size();
  while(end>0&&(value[end-1]==' '||value[end-1]=='\t'))--end;
  return value.substr(0,end);
}
std::string parseValue(const std::string& line,std::size_t start){
  std::string result;bool inQuote=false;std::size_t index=start;
  while(index<line.size()){
    const char ch=line[index];
    if(!inQuote&&(ch=='#'||ch==';'))break;
    if(ch=='"'){inQuote=!inQuote;++index;continue;}
    if(ch=='\\'&&index+1<line.size()){
      const char next=line[index+1];
      if(inQuote){
        switch(next){case 'n':result+='\n';break;case 't':result+='\t';break;
          case 'b':result+='\b';break;default:result+=next;break;}
        index+=2;continue;
      }
      if(next=='\\'){result+='\\';index+=2;continue;}
    }
    result+=ch;++index;
  }
  return inQuote?result:trimTrailingWhitespace(result);
}
std::optional<std::pair<std::string,std::string>> parseKeyValue(const std::string& line){
  std::size_t index=0;while(index<line.size()&&keyChar(line[index]))++index;
  if(index==0)return std::nullopt;
  const auto key=line.substr(0,index);
  while(index<line.size()&&(line[index]==' '||line[index]=='\t'))++index;
  if(index>=line.size()||line[index]!='=')return std::nullopt;
  ++index;
  while(index<line.size()&&(line[index]==' '||line[index]=='\t'))++index;
  return std::pair(key,parseValue(line,index));
}
bool matchesSectionHeader(const std::string& line,const std::string& sectionLower,
  const std::optional<std::string>& subsection){
  std::size_t index=1;
  while(index<line.size()&&line[index]!=']'&&line[index]!=' '&&
    line[index]!='\t'&&line[index]!='"')++index;
  if(lower(line.substr(1,index-1))!=sectionLower)return false;
  if(!subsection)return index<line.size()&&line[index]==']';
  while(index<line.size()&&(line[index]==' '||line[index]=='\t'))++index;
  if(index>=line.size()||line[index]!='"')return false;
  ++index;std::string found;
  while(index<line.size()&&line[index]!='"'){
    if(line[index]=='\\'&&index+1<line.size()){
      found+=line[index+1];index+=2;continue;
    }
    found+=line[index++];
  }
  if(index>=line.size()||line[index]!='"')return false;
  ++index;if(index>=line.size()||line[index]!=']')return false;
  return found==*subsection;
}
}
std::optional<std::string> parseConfigString(const std::string& config,
  const std::string& section,const std::optional<std::string>& subsection,
  const std::string& key){
  const auto sectionLower=lower(section),keyLower=lower(key);
  bool inSection=false;std::size_t start=0;
  for(;;){
    const auto end=config.find('\n',start);
    const auto line=value::trim(config.substr(start,end==std::string::npos?
      std::string::npos:end-start));
    if(!line.empty()&&line[0]!='#'&&line[0]!=';'){
      if(line[0]=='[')inSection=matchesSectionHeader(line,sectionLower,subsection);
      else if(inSection){
        const auto parsed=parseKeyValue(line);
        if(parsed&&lower(parsed->first)==keyLower)return parsed->second;
      }
    }
    if(end==std::string::npos)break;
    start=end+1;
  }
  return std::nullopt;
}
Deferred<std::optional<std::string>> parseGitConfigValue(event_loop::Loop& loop,
  const Services& services,const std::string& gitDir,const std::string& section,
  const std::optional<std::string>& subsection,const std::string& key){
  Deferred<std::optional<std::string>> output(loop);
  try{services.readFile(joinPath(gitDir,"config")).then(
    [output,section,subsection,key](const std::string* config,
      const std::exception_ptr& error){
      if(error){output.resolve(std::nullopt);return;}
      try{output.resolve(parseConfigString(*config,section,subsection,key));}
      catch(...){output.resolve(std::nullopt);}
    });}catch(...){output.resolve(std::nullopt);}
  return output;
}
void bindIndigenousConfigParser(event_loop::Loop& loop,Services& services){
  if(!services.readFile)throw std::invalid_argument("Git config readFile import required");
  const auto read=services.readFile;
  services.parseGitConfigValue=[&loop,read](const std::string& gitDir,
    const std::string& section,const std::optional<std::string>& subsection,
    const std::string& key){
    Services operations;operations.readFile=read;
    return parseGitConfigValue(loop,operations,gitDir,section,subsection,key);
  };
}
} // namespace batbox::git_filesystem
