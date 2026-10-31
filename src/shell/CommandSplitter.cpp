#include "shell/CommandSplitter.hpp"
#include "shell/Heredoc.hpp"
#include "shell/RandomSalt.hpp"
#include "shell/ShellQuote.hpp"
#include "runtime/JsonValue.hpp"
#include <optional>
#include <stdexcept>

namespace batbox::shell_split {
namespace {
struct Placeholders {
  std::string single,doubleQuote,newline,openParen,closeParen;
};
std::string replaceAll(std::string text,const std::string& from,const std::string& to){
  if(from.empty())return text;
  std::size_t at=0;
  while((at=text.find(from,at))!=std::string::npos){text.replace(at,from.size(),to);at+=to.size();}
  return text;
}
std::string joinContinuations(const std::string& source){
  std::string result;
  for(std::size_t i=0;i<source.size();){
    if(source[i]!='\\'){result+=source[i++];continue;}
    const auto start=i;while(i<source.size()&&source[i]=='\\')++i;
    const auto count=i-start;
    if(i<source.size()&&source[i]=='\n'&&count%2==1){
      result.append(count-1,'\\');++i;
    }else result.append(source,start,count);
  }
  return result;
}
std::string salt(const SaltProvider& provider){
  if(provider)return provider();
  return shell::randomPlaceholderSalt();
}
Placeholders placeholders(const std::string& value){
  return {"__SINGLE_QUOTE_"+value+"__","__DOUBLE_QUOTE_"+value+"__",
    "__NEW_LINE_"+value+"__","__ESCAPED_OPEN_PAREN_"+value+"__",
    "__ESCAPED_CLOSE_PAREN_"+value+"__"};
}
std::string restored(std::string text,const Placeholders& markers){
  text=replaceAll(std::move(text),markers.single,"'");
  text=replaceAll(std::move(text),markers.doubleQuote,"\"");
  text=replaceAll(std::move(text),"\n"+markers.newline+"\n","\n");
  text=replaceAll(std::move(text),markers.openParen,"\\(");
  return replaceAll(std::move(text),markers.closeParen,"\\)");
}
bool isString(const std::optional<Json>& part){return part&&part->is_string();}
}
std::vector<std::string> splitCommandWithOperators(const std::string& command,
  const SaltProvider& saltProvider){
  const auto markers=placeholders(salt(saltProvider));
  const auto extracted=shell_heredoc::extractHeredocs(command,false,saltProvider);
  const auto& processedCommand=extracted.processedCommand;
  const auto joined=joinContinuations(processedCommand);
  const auto originalJoined=joinContinuations(command);
  auto quoted=replaceAll(joined,"\"","\""+markers.doubleQuote);
  quoted=replaceAll(std::move(quoted),"'","'"+markers.single);
  quoted=replaceAll(std::move(quoted),"\n","\n"+markers.newline+"\n");
  quoted=replaceAll(std::move(quoted),"\\(",markers.openParen);
  quoted=replaceAll(std::move(quoted),"\\)",markers.closeParen);
  Json parsed;
  try{parsed=shell_quote::parse(quoted,[](const std::string& name)->std::optional<Json>{
    return Json("$"+name);});}
  catch(...){return {originalJoined};}
  if(parsed.empty())return {};
  try{
    std::vector<std::optional<Json>> parts;
    for(const auto& part:parsed){
      if(part.is_string()&&!parts.empty()&&isString(parts.back())){
        const auto value=part.get<std::string>();
        if(value==markers.newline)parts.push_back(std::nullopt);
        else parts.back()=Json(parts.back()->get<std::string>()+" "+value);
        continue;
      }
      if(part.is_object()&&part.value("op",std::string())=="glob"&&
         !parts.empty()&&isString(parts.back())){
        parts.back()=Json(parts.back()->get<std::string>()+" "+part.at("pattern").get<std::string>());
        continue;
      }
      parts.push_back(part);
    }
    std::vector<std::string> result;
    for(const auto& part:parts){
      if(!part)continue;
      std::string value;
      if(part->is_string())value=part->get<std::string>();
      else if(part->is_object()&&part->contains("comment")){
        auto comment=part->at("comment").get<std::string>();
        comment=replaceAll(std::move(comment),"\""+markers.doubleQuote,markers.doubleQuote);
        comment=replaceAll(std::move(comment),"'"+markers.single,markers.single);
        value="#"+comment;
      }else if(part->is_object()&&part->value("op",std::string())=="glob")
        value=part->at("pattern").get<std::string>();
      else if(part->is_object()&&part->contains("op"))value=part->at("op").get<std::string>();
      else continue;
      result.push_back(restored(std::move(value),markers));
    }
    return shell_heredoc::restoreHeredocs(result,extracted.heredocs);
  }catch(...){return {originalJoined};}
}
} // namespace batbox::shell_split
