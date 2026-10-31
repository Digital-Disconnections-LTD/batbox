#include "history/References.hpp"
#include <cstdlib>
#include <regex>

namespace batbox::history {
std::size_t getPastedTextRefNumLines(const std::string& text){
  std::size_t count=0;
  for(std::size_t i=0;i<text.size();++i){
    if(text[i]=='\r'){
      ++count;
      if(i+1<text.size()&&text[i+1]=='\n')++i;
    }else if(text[i]=='\n')++count;
  }
  return count;
}
std::string formatPastedTextRef(double id,double numLines){
  const auto prefix="[Pasted text #"+value::numberToString(id);
  if(numLines==0)return prefix+"]";
  return prefix+" +"+value::numberToString(numLines)+" lines]";
}
std::string formatImageRef(double id){
  return "[Image #"+value::numberToString(id)+"]";
}
std::vector<Reference> parseReferences(const std::string& input){
  static const std::regex pattern(
    R"(\[(Pasted text|Image|\.\.\.Truncated text) #([0-9]+)( \+[0-9]+ lines)?(\.)*\])",
    std::regex::ECMAScript);
  std::vector<Reference> result;
  for(auto it=std::sregex_iterator(input.begin(),input.end(),pattern);
      it!=std::sregex_iterator();++it){
    const auto number=std::strtod((*it)[2].str().c_str(),nullptr);
    if(!(number>0))continue;
    result.push_back({number,it->str(),
      value::stringLength(std::string_view(input).substr(0,it->position()))});
  }
  return result;
}
std::string expandPastedTextRefs(const std::string& input,const Json& pastedContents){
  const auto refs=parseReferences(input);
  std::string expanded=input;
  for(auto it=refs.rbegin();it!=refs.rend();++it){
    const auto key=value::numberToString(it->id);
    if(!pastedContents.is_object()||!pastedContents.contains(key))continue;
    const auto& content=pastedContents.at(key);
    if(!content.is_object()||value::property(content,"type")!="text")continue;
    const auto replacement=value::property(content,"content");
    const auto value=replacement.is_string()?replacement.get<std::string>():
      replacement.is_null()?std::string("undefined"):value::stringify(replacement);
    expanded=value::slice(expanded,0,it->index)+value+
      value::slice(expanded,it->index+value::stringLength(it->match));
  }
  return expanded;
}
}
