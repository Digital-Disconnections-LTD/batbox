#include "glob/PicomatchParser.hpp"
#include "glob/PicomatchSafety.hpp"
#include "glob/Regex16.hpp"
#include <algorithm>

namespace batbox::glob::detail {
namespace {
const std::u16string slash=u"\\/";
const std::u16string oneChar=u"(?=.)";
const std::u16string star=u"[^/]*?";
const std::u16string noDotsSlash=u"(?!\\.{1,2}(?:\\/|$))";
const std::u16string noDotSlash=u"(?!\\.{0,1}(?:\\/|$))";
std::u16string globstar(){
  return u"(?:(?:(?!(?:^|\\/)\\.{1,2}(?:\\/|$)).)*?)";
}
}
void PicoParser::extglobOpen(const std::string& type,char16_t value){
  PicoExtglob token;
  token.type=type;token.prev=prev_;token.parens=parens_;
  token.output=output_;token.startIndex=static_cast<std::size_t>(index_);
  token.tokensIndex=tokens_.size();
  if(type=="negate"){
    token.open=u"(?:(?!(?:";token.close=u"))"+star+u")";
  }else if(type=="qmark"){
    token.open=u"(?:";token.close=u")?";
  }else if(type=="plus"){
    token.open=u"(?:";token.close=u")+";
  }else if(type=="star"){
    token.open=u"(?:";token.close=u")*";
  }
  increment("parens");
  push(PicoToken{type,std::u16string(1,value),
    output_.empty()?std::optional<std::u16string>(oneChar):std::u16string(u"")});
  push(PicoToken{"paren",std::u16string(1,advance()),token.open,{},0,false,false,true});
  extglobs_.push_back(std::move(token));
}
void PicoParser::extglobClose(PicoExtglob token){
  const auto literal=input_.substr(token.startIndex,
    static_cast<std::size_t>(index_)-token.startIndex+1);
  const auto body=input_.substr(token.startIndex+2,
    static_cast<std::size_t>(index_)-token.startIndex-2);
  const auto analysis=analyzeRepeatedExtglob(body);
  if((token.type=="plus"||token.type=="star")&&analysis.risky){
    auto& open=tokens_[token.tokensIndex];
    open.type="text";open.value=literal;
    open.output=analysis.safeOutput
      ?(token.output.empty()?oneChar:std::u16string{})+*analysis.safeOutput
      :escapePicoRegex(literal);
    for(std::size_t i=token.tokensIndex+1;i<tokens_.size();i++){
      tokens_[i].value.clear();tokens_[i].output=std::u16string(u"");
      tokens_[i].suffix.clear();
    }
    output_=token.output+*open.output;backtrack_=true;
    push(PicoToken{"paren",u")",std::u16string(u""),{},0,false,false,true});
    decrement("parens");return;
  }
  std::u16string output=token.close;
  if(token.type=="negate"){
    auto extglobStar=star;
    if(token.inner.size()>1&&token.inner.find(u'/')!=std::u16string::npos)
      extglobStar=globstar();
    const auto rest=remaining();
    bool onlyParens=!rest.empty();
    for(const auto ch:rest)if(ch!=u')')onlyParens=false;
    if(extglobStar!=star||eos()||onlyParens)
      output=u")$))"+extglobStar;
    bool suffix=rest.size()>1&&rest[0]==u'.';
    for(std::size_t i=1;i<rest.size();i++)if(rest[i]==u'/'||rest[i]==u'.')suffix=false;
    if(token.inner.find(u'*')!=std::u16string::npos&&suffix){
      const auto expression=PicoParser(rest,dot_).parse().output;
      output=u")"+expression+u")"+extglobStar+u")";
    }
    if(tokens_[token.prev].type=="bos")negatedExtglob_=true;
  }
  push(PicoToken{"paren",u")",output,{},0,false,false,true});
  decrement("parens");
}
bool PicoParser::handleStar(){
  const std::u16string value=u"*";
  if(prev().type=="globstar"||prev().star){
    prev().type="star";prev().star=true;prev().value+=value;
    prev().output=star;backtrack_=true;globstar_=true;consume(value);
    return true;
  }
  auto rest=remaining();
  if(rest.size()>1&&rest[0]==u'('&&rest[1]!=u'?'){
    extglobOpen("star",u'*');return true;
  }
  if(prev().type=="star"){
    const auto priorIndex=prev().prev;
    const auto beforeIndex=tokens_[priorIndex].prev;
    const bool isStart=tokens_[priorIndex].type=="slash"||tokens_[priorIndex].type=="bos";
    const bool afterStar=tokens_[priorIndex].type!="bos"&&
      (tokens_[beforeIndex].type=="star"||tokens_[beforeIndex].type=="globstar");
    const bool isBrace=bracesCount_>0&&
      (tokens_[priorIndex].type=="comma"||tokens_[priorIndex].type=="brace");
    const bool isExtglob=!extglobs_.empty()&&
      (tokens_[priorIndex].type=="pipe"||tokens_[priorIndex].type=="paren");
    if(!isStart&&tokens_[priorIndex].type!="paren"&&!isBrace&&!isExtglob){
      push(PicoToken{"star",value,std::u16string(u"")});return true;
    }
    while(rest.starts_with(u"/**")){
      const auto position=static_cast<std::size_t>(index_+4);
      const auto after=position<input_.size()?input_[position]:0;
      if(after&&after!=u'/')break;
      rest.erase(0,3);consume(u"/**",3);
    }
    if(tokens_[priorIndex].type=="bos"&&eos()){
      prev().type="globstar";prev().value+=value;
      prev().output=globstar();output_=*prev().output;
      globstar_=true;consume(value);return true;
    }
    if(tokens_[priorIndex].type=="slash"&&
      tokens_[priorIndex].prev!=0&&!afterStar&&eos()){
      const auto count=tokens_[priorIndex].output->size()+prev().output->size();
      output_.erase(output_.size()-count);
      tokens_[priorIndex].output=u"(?:"+*tokens_[priorIndex].output;
      prev().type="globstar";prev().output=globstar()+u"|$)";
      prev().value+=value;globstar_=true;
      output_+=*tokens_[priorIndex].output+*prev().output;
      consume(value);return true;
    }
    if(tokens_[priorIndex].type=="slash"&&
      tokens_[priorIndex].prev!=0&&!rest.empty()&&rest[0]==u'/'){
      const auto end=rest.size()>1?u"|$":u"";
      const auto count=tokens_[priorIndex].output->size()+prev().output->size();
      output_.erase(output_.size()-count);
      tokens_[priorIndex].output=u"(?:"+*tokens_[priorIndex].output;
      prev().type="globstar";
      prev().output=globstar()+slash+u"|"+slash+end+u")";
      prev().value+=value;output_+=*tokens_[priorIndex].output+*prev().output;
      globstar_=true;consume(u"*/",0);advance();
      push(PicoToken{"slash",u"/",std::u16string(u"")});return true;
    }
    if(tokens_[priorIndex].type=="bos"&&!rest.empty()&&rest[0]==u'/'){
      prev().type="globstar";prev().value+=value;
      prev().output=u"(?:^|"+slash+u"|"+globstar()+slash+u")";
      output_=*prev().output;globstar_=true;
      consume(u"*/",0);advance();
      push(PicoToken{"slash",u"/",std::u16string(u"")});return true;
    }
    output_.erase(output_.size()-prev().output->size());
    prev().type="globstar";prev().output=globstar();prev().value+=value;
    output_+=*prev().output;globstar_=true;consume(value);return true;
  }
  if(index_==static_cast<std::ptrdiff_t>(start_)||prev().type=="slash"||prev().type=="dot"){
    const auto prefix=prev().type=="dot"?noDotSlash:noDotsSlash;
    output_+=prefix;prev().output=prev().output.value_or(prev().value)+prefix;
    if(peek()!=u'*'){
      output_+=oneChar;*prev().output+=oneChar;
    }
  }
  push(PicoToken{"star",value,star});return true;
}
} // namespace batbox::glob::detail
