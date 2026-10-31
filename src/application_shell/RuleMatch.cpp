#include "application_shell/RuleMatch.hpp"
#include "glob/Regex16.hpp"

namespace batbox::application_shell {
std::optional<std::string> permissionRuleExtractPrefix(const std::string& rule){
  if(rule.size()<=2||!rule.ends_with(":*"))return std::nullopt;
  return rule.substr(0,rule.size()-2);
}
bool hasWildcards(const std::string& pattern){
  if(pattern.ends_with(":*"))return false;
  for(std::size_t i=0;i<pattern.size();++i)if(pattern[i]=='*'){
    std::size_t escapes=0;
    for(auto at=i;at>0&&pattern[at-1]=='\\';--at)++escapes;
    if(escapes%2==0)return true;
  }
  return false;
}
bool matchWildcardPattern(const std::string& pattern,const std::string& command,
    bool caseInsensitive){
  const auto trimmed=glob::units(value::trim(pattern));
  std::u16string output=u"(?s)^";
  std::size_t stars=0;
  for(std::size_t i=0;i<trimmed.size();++i){
    if(trimmed[i]==u'\\'&&i+1<trimmed.size()&&
       (trimmed[i+1]==u'*'||trimmed[i+1]==u'\\')){++i;continue;}
    if(trimmed[i]==u'*')++stars;
  }
  const bool optionalTrailing=stars==1&&trimmed.size()>=2&&
    trimmed[trimmed.size()-2]==u' '&&trimmed.back()==u'*';
  const auto stop=optionalTrailing?trimmed.size()-2:trimmed.size();
  for(std::size_t i=0;i<stop;++i){
    const char16_t c=trimmed[i];
    if(c==u'\\'&&i+1<stop&&(trimmed[i+1]==u'*'||trimmed[i+1]==u'\\')){
      output+=u'\\';output+=trimmed[++i];continue;
    }
    if(c==u'*'){output+=u".*";continue;}
    if(c==u'.'||c==u'+'||c==u'?'||c==u'^'||c==u'$'||c==u'{'||
       c==u'}'||c==u'('||c==u')'||c==u'|'||c==u'['||c==u']'||
       c==u'\\')output+=u'\\';
    output+=c;
  }
  if(optionalTrailing)output+=u"( .*)?";
  output+=u'$';
  return glob::Regex16(std::move(output),caseInsensitive).search(glob::units(command));
}
ShellRule parsePermissionRule(const std::string& rule){
  if(const auto prefix=permissionRuleExtractPrefix(rule))
    return {ShellRule::Type::Prefix,*prefix};
  if(hasWildcards(rule))return {ShellRule::Type::Wildcard,rule};
  return {ShellRule::Type::Exact,rule};
}
Json suggestionForExactCommand(const std::string& name,const std::string& command){
  return Json::array({Json{{"type","addRules"},
    {"rules",Json::array({Json{{"toolName",name},{"ruleContent",command}}})},
    {"behavior","allow"},{"destination","localSettings"}}});
}
Json suggestionForPrefix(const std::string& name,const std::string& prefix){
  return suggestionForExactCommand(name,prefix+":*");
}
} // namespace batbox::application_shell
