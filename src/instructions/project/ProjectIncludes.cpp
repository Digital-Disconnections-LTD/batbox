#include "instructions/project/ProjectMemory.hpp"
#include "runtime/JsonValue.hpp"
#include <unicode/regex.h>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <unordered_set>

namespace batbox::instructions::project {
namespace {
std::string str(const Json& value){
  return value.is_string()?value.get<std::string>():std::string{};
}
std::string removeCommentSpans(const std::string& raw){
  std::string output;std::size_t from=0;
  while(from<raw.size()){
    const auto start=raw.find("<!--",from);
    if(start==std::string::npos){output+=raw.substr(from);break;}
    output+=raw.substr(from,start-from);
    const auto end=raw.find("-->",start+4);
    if(end==std::string::npos){output+=raw.substr(start);break;}
    from=end+3;
  }
  return output;
}
std::vector<std::string> extractPaths(const std::string& text){
  static const auto pattern=[](){
    UErrorCode status=U_ZERO_ERROR;
    auto* compiled=icu::RegexPattern::compile(
      icu::UnicodeString::fromUTF8(
        R"((?:^|[\s\uFEFF])@((?:[^\s\uFEFF\\]|\\ )+))"),0,status);
    if(U_FAILURE(status)||!compiled)throw std::runtime_error("Include regex failed");
    return std::unique_ptr<icu::RegexPattern>(compiled);
  }();
  const auto units=value::utf16CodeUnits(text);
  const icu::UnicodeString unicode(reinterpret_cast<const UChar*>(units.data()),
    static_cast<int32_t>(units.size()));
  UErrorCode status=U_ZERO_ERROR;
  std::unique_ptr<icu::RegexMatcher> matcher(pattern->matcher(unicode,status));
  if(U_FAILURE(status))throw std::runtime_error("Include regex matcher failed");
  std::vector<std::string> found;
  while(matcher->find(status)){
    auto group=matcher->group(1,status);
    if(U_FAILURE(status))throw std::runtime_error("Include regex group failed");
    std::u16string value;
    for(int32_t i=0;i<group.length();++i)value.push_back(group.charAt(i));
    found.push_back(value::fromUtf16(value));
  }
  if(U_FAILURE(status))throw std::runtime_error("Include regex scan failed");
  return found;
}
bool starts(const std::string& value,const char* prefix){
  return value.rfind(prefix,0)==0;
}
bool validIncludePath(const std::string& path){
  if(starts(path,"./")||starts(path,"~/"))return true;
  if(starts(path,"/")&&path!="/")return true;
  if(path.empty()||path[0]=='@')return false;
  const unsigned char first=static_cast<unsigned char>(path[0]);
  return (first>='a'&&first<='z')||(first>='A'&&first<='Z')||
    (first>='0'&&first<='9')||first=='.'||first=='_'||first=='-';
}
void replaceEscapedSpaces(std::string& path){
  std::size_t index=0;
  while((index=path.find("\\ ",index))!=std::string::npos){
    path.replace(index,2," ");++index;
  }
}
}
std::vector<std::string> extractIncludePathsFromTokens(const Json& tokens,
  const std::string& basePath,const ParseServices& services){
  std::vector<std::string> absolutePaths;
  std::unordered_set<std::string> seen;
  const auto baseDir=std::filesystem::path(basePath).parent_path().string();
  const auto extractText=[&](const std::string& text){
    for(auto path:extractPaths(text)){
      if(path.empty())continue;
      const auto hash=path.find('#');
      if(hash!=std::string::npos)path.resize(hash);
      if(path.empty())continue;
      replaceEscapedSpaces(path);
      if(!validIncludePath(path))continue;
      const auto resolved=filesystem::expandPath(path,services.paths,baseDir);
      if(seen.insert(resolved).second)absolutePaths.push_back(resolved);
    }
  };
  std::function<void(const Json&)> visit=[&](const Json& elements){
    if(!elements.is_array())return;
    for(const auto& element:elements){
      const auto type=str(value::property(element,"type"));
      if(type=="code"||type=="codespan")continue;
      if(type=="html"){
        const auto raw=str(value::property(element,"raw"));
        const auto trimmed=value::trim(raw);
        if(trimmed.starts_with("<!--")&&trimmed.find("-->")!=std::string::npos){
          const auto residue=removeCommentSpans(raw);
          if(!value::trim(residue).empty())extractText(residue);
        }
        continue;
      }
      if(type=="text")extractText(str(value::property(element,"text")));
      visit(value::property(element,"tokens"));
      visit(value::property(element,"items"));
    }
  };
  visit(tokens);
  return absolutePaths;
}
}
