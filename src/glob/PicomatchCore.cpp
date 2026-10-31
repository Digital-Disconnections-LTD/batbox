#include "glob/PicomatchParser.hpp"
#include "glob/Regex16.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::glob::detail {
namespace {
constexpr std::u16string_view star=u"[^/]*?";
bool special(char16_t c){
  switch(c){case u'-':case u'*':case u'+':case u'?':case u'.':
  case u'^':case u'$':case u'{':case u'}':case u'(':
  case u'|':case u')':case u'[':case u']':case u'\\':return true;
  default:return false;}
}
}
std::u16string escapePicoRegex(std::u16string_view value){
  std::u16string output;
  for(const auto ch:value){if(special(ch))output+=u'\\';output+=ch;}
  return output;
}
std::u16string escapePicoLast(std::u16string value,char16_t character){
  auto position=value.find_last_of(character);
  while(position!=std::u16string::npos){
    if(position==0||value[position-1]!=u'\\'){
      value.insert(position,1,u'\\');break;
    }
    if(position==0)break;
    position=value.find_last_of(character,position-1);
  }
  return value;
}
PicoParser::PicoParser(std::u16string pattern,bool dot):input_(std::move(pattern)),dot_(dot){
  if(input_==u"***")input_=u"*";
  else if(input_==u"**/**"||input_==u"**/**/**")input_=u"**";
  if(input_.size()>65536)throw std::length_error("Input length: "+
    std::to_string(input_.size())+", exceeds maximum allowed length: 65536");
  if(input_.starts_with(u"./")){input_.erase(0,2);start_=0;}
  tokens_.push_back(PicoToken{"bos",u"",std::u16string(u"")});
}
bool PicoParser::eos()const{return index_==static_cast<std::ptrdiff_t>(input_.size())-1;}
char16_t PicoParser::peek(std::size_t n)const{
  const auto position=index_+static_cast<std::ptrdiff_t>(n);
  return position>=0&&position<static_cast<std::ptrdiff_t>(input_.size())
    ?input_[position]:0;
}
char16_t PicoParser::advance(){
  index_++;return index_<static_cast<std::ptrdiff_t>(input_.size())?input_[index_]:0;
}
std::u16string PicoParser::remaining()const{
  return input_.substr(static_cast<std::size_t>(index_+1));
}
void PicoParser::consume(std::u16string_view value,std::size_t count){
  consumed_+=value;index_+=static_cast<std::ptrdiff_t>(count);
}
void PicoParser::append(const PicoToken& token){
  output_+=token.output.value_or(token.value);
  consume(token.value);
}
void PicoParser::push(PicoToken token){
  if(prev().type=="globstar"){
    const bool isBrace=bracesCount_>0&&(token.type=="comma"||token.type=="brace");
    const bool isExtglob=token.extglob||(!extglobs_.empty()&&
      (token.type=="pipe"||token.type=="paren"));
    if(token.type!="slash"&&token.type!="paren"&&!isBrace&&!isExtglob){
      auto& previous=prev();
      const auto count=previous.output.value_or(u"").size();
      if(count<=output_.size())output_.erase(output_.size()-count);
      previous.type="star";previous.value=u"*";previous.output=std::u16string(star);
      output_+=*previous.output;
    }
  }
  if(!extglobs_.empty()&&token.type!="paren")extglobs_.back().inner+=token.value;
  if(!token.value.empty()||(token.output&&!token.output->empty()))append(token);
  if(prev().type=="text"&&token.type=="text"){
    auto& previous=prev();
    previous.output=(previous.output&&!previous.output->empty())
      ?*previous.output+token.value:previous.value+token.value;
    previous.value+=token.value;
    return;
  }
  token.prev=prev_;
  tokens_.push_back(std::move(token));prev_=tokens_.size()-1;
}
void PicoParser::increment(const char* type){
  if(std::string_view(type)=="brackets")brackets_++;
  else if(std::string_view(type)=="braces")bracesCount_++;
  else parens_++;
  stack_.emplace_back(type);
}
void PicoParser::decrement(const char* type){
  if(std::string_view(type)=="brackets")brackets_--;
  else if(std::string_view(type)=="braces")bracesCount_--;
  else parens_--;
  if(!stack_.empty())stack_.pop_back();
}
void PicoParser::closeUnbalanced(){
  while(brackets_>0){output_=escapePicoLast(std::move(output_),u'[');decrement("brackets");}
  while(parens_>0){output_=escapePicoLast(std::move(output_),u'(');decrement("parens");}
  while(bracesCount_>0){output_=escapePicoLast(std::move(output_),u'{');decrement("braces");}
  if(prev().type=="star"||prev().type=="bracket")
    push(PicoToken{"maybe_slash",u"",std::u16string(u"\\/?")});
  if(backtrack_){
    output_.clear();
    for(const auto& token:tokens_){
      output_+=token.output.value_or(token.value);
      output_+=token.suffix;
    }
  }
}
PicoState PicoParser::parse(){
  while(!eos()){
    std::u16string value(1,advance());
    if(value==u"\0")continue;
    if(value==u"\\"&&handleEscape(value))continue;
    if(brackets_>0&&handleBracketBody(value))continue;
    if(quotes_==1&&value!=u"\""){
      value=escapePicoRegex(value);prev().value+=value;
      append(PicoToken{"",value});continue;
    }
    if(value==u"\""){quotes_=quotes_==1?0:1;continue;}
    if(handleGroup(value[0]))continue;
    if(handleBrace(value[0]))continue;
    if(value==u"*"){handleStar();continue;}
    handleSpecial(value[0]);
  }
  closeUnbalanced();
  return PicoState{output_,negated_,globstar_,negatedExtglob_};
}
} // namespace batbox::glob::detail
