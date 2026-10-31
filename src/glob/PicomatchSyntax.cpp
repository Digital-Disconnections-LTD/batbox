#include "glob/PicomatchParser.hpp"
#include "glob/Regex16.hpp"
#include <algorithm>
#include <map>
#include <stdexcept>

namespace batbox::glob::detail {
namespace {
const std::map<std::u16string,std::u16string> posixClasses={
  {u"alnum",u"a-zA-Z0-9"},{u"alpha",u"a-zA-Z"},
  {u"ascii",u"\\x00-\\x7F"},{u"blank",u" \\t"},
  {u"cntrl",u"\\x00-\\x1F\\x7F"},{u"digit",u"0-9"},
  {u"graph",u"\\x21-\\x7E"},{u"lower",u"a-z"},
  {u"print",u"\\x20-\\x7E "},
  {u"punct",u"\\-!\"#$%&'()\\*+,./:;<=>?@[\\]^_`{|}~"},
  {u"space",u" \\t\\r\\n\\v\\f"},{u"upper",u"A-Z"},
  {u"word",u"A-Za-z0-9_"},{u"xdigit",u"A-Fa-f0-9"}};
bool hasRegexChars(std::u16string_view value){
  for(const auto ch:value)if(ch==u'-'||ch==u'*'||ch==u'+'||ch==u'?'||
    ch==u'.'||ch==u'^'||ch==u'$'||ch==u'{'||ch==u'}'||
    ch==u'('||ch==u'|'||ch==u')'||ch==u'['||ch==u']'||ch==u'\\')return true;
  return false;
}
}
bool PicoParser::handleEscape(std::u16string& value){
  const auto next=peek();
  if(next==u'/'||next==u'.'||next==u';')return true;
  if(!next){value+=u'\\';push(PicoToken{"text",value});return true;}
  std::size_t slashes=0;
  while(static_cast<std::size_t>(index_+1+slashes)<input_.size()&&
    input_[index_+1+slashes]==u'\\')slashes++;
  if(slashes>2){index_+=static_cast<std::ptrdiff_t>(slashes);
    if(slashes%2)value+=u'\\';}
  value+=advance();
  if(brackets_==0){push(PicoToken{"text",value});return true;}
  return false;
}
bool PicoParser::handleBracketBody(std::u16string& value){
  if(value==u"]"&&prev().value!=u"["&&prev().value!=u"[^")return false;
  if(value==u":"){
    const auto inner=prev().value.substr(1);
    if(inner.find(u'[')!=std::u16string::npos){
      prev().posix=true;
      if(inner.find(u':')!=std::u16string::npos){
        const auto position=prev().value.find_last_of(u'[');
        const auto prefix=prev().value.substr(0,position);
        const auto rest=prev().value.substr(position+2);
        const auto found=posixClasses.find(rest);
        if(found!=posixClasses.end()){
          prev().value=prefix+found->second;backtrack_=true;
          advance();
          if(tokens_[0].output->empty()&&prev_==1)
            tokens_[0].output=u"(?=.)";
          return true;
        }
      }
    }
  }
  if((value==u"["&&peek()!=u':')||(value==u"-"&&peek()==u']'))
    value=u"\\"+value;
  if(value==u"]"&&(prev().value==u"["||prev().value==u"[^"))
    value=u"\\"+value;
  prev().value+=value;append(PicoToken{"",value});return true;
}
bool PicoParser::handleGroup(char16_t value){
  if(value==u'('){increment("parens");push(PicoToken{"paren",u"("});return true;}
  if(value==u')'){
    if(!extglobs_.empty()&&parens_==extglobs_.back().parens+1){
      const auto token=extglobs_.back();extglobs_.pop_back();
      extglobClose(token);return true;
    }
    push(PicoToken{"paren",u")",parens_?u")":u"\\)"});
    decrement("parens");return true;
  }
  if(value==u'['){
    if(remaining().find(u']')==std::u16string::npos){
      push(PicoToken{"bracket",u"[",u"\\["});
    }else{increment("brackets");push(PicoToken{"bracket",u"["});}
    return true;
  }
  if(value==u']'){
    if(prev().type=="bracket"&&prev().value.size()==1){
      push(PicoToken{"text",u"]",u"\\]"});return true;
    }
    if(brackets_==0){push(PicoToken{"text",u"]",u"\\]"});return true;}
    decrement("brackets");
    const auto previous=prev().value.substr(1);
    std::u16string closing=u"]";
    if(!prev().posix&&previous.starts_with(u"^")&&
      previous.find(u'/')==std::u16string::npos)closing=u"/]";
    prev().value+=closing;append(PicoToken{"",closing});
    if(hasRegexChars(previous))return true;
    const auto escaped=escapePicoRegex(prev().value);
    const auto count=prev().value.size();
    if(count<=output_.size())output_.erase(output_.size()-count);
    prev().value=u"(?:"+escaped+u"|"+prev().value+u")";
    output_+=prev().value;return true;
  }
  return false;
}
bool PicoParser::handleBrace(char16_t value){
  if(value==u'{'){
    increment("braces");
    const auto tokenIndex=tokens_.size(),outputIndex=output_.size();
    push(PicoToken{"brace",u"{",u"("});
    braces_.push_back(PicoBrace{tokenIndex,outputIndex});
    return true;
  }
  if(value==u'}'){
    if(braces_.empty()){push(PicoToken{"text",u"}"});return true;}
    auto brace=braces_.back();braces_.pop_back();
    std::u16string closing=u")",valueText=u"}";
    if(brace.dots){
      std::vector<std::u16string> range;
      for(std::size_t i=tokens_.size();i>brace.tokenIndex+1;){
        --i;if(tokens_[i].type!="dots")range.insert(range.begin(),tokens_[i].value);
      }
      tokens_.resize(brace.tokenIndex+1);prev_=brace.tokenIndex;
      tokens_[brace.tokenIndex].value.clear();
      tokens_[brace.tokenIndex].output=std::u16string(u"");
      std::sort(range.begin(),range.end());
      std::u16string candidate=u"[";
      for(std::size_t i=0;i<range.size();i++){
        if(i)candidate+=u'-';
        candidate+=range[i];
      }
      candidate+=u']';
      try{Regex16 test(candidate);closing=candidate;}
      catch(...){
        closing.clear();
        for(std::size_t i=0;i<range.size();i++){
          if(i)closing+=u"..";
          closing+=escapePicoRegex(range[i]);
        }
      }
      backtrack_=true;
    }
    if(!brace.comma&&!brace.dots){
      const auto prefix=output_.substr(0,brace.outputIndex);
      tokens_[brace.tokenIndex].value=u"\\{";
      tokens_[brace.tokenIndex].output=u"\\{";
      valueText=closing=u"\\}";
      output_=prefix;
      for(std::size_t i=brace.tokenIndex;i<tokens_.size();i++)
        output_+=tokens_[i].output.value_or(tokens_[i].value);
    }
    push(PicoToken{"brace",valueText,closing});
    decrement("braces");return true;
  }
  return false;
}
bool PicoParser::handleSpecial(char16_t value){
  const std::u16string string(1,value);
  if(value==u'|'){
    if(!extglobs_.empty())extglobs_.back().conditions++;
    push(PicoToken{"text",string});return true;
  }
  if(value==u','){
    auto output=string;
    if(!braces_.empty()&&!stack_.empty()&&stack_.back()=="braces"){
      braces_.back().comma=true;output=u"|";
    }
    push(PicoToken{"comma",string,output});return true;
  }
  if(value==u'/'){
    if(prev().type=="dot"&&index_==static_cast<std::ptrdiff_t>(start_+1)){
      start_=static_cast<std::size_t>(index_+1);
      consumed_.clear();output_.clear();tokens_.resize(1);prev_=0;
    }else push(PicoToken{"slash",u"/",u"\\/"});
    return true;
  }
  if(value==u'.'){
    if(bracesCount_>0&&prev().type=="dot"){
      if(prev().value==u".")prev().output=u"\\.";
      prev().type="dots";*prev().output+=u'.';prev().value+=u'.';
      braces_.back().dots=true;return true;
    }
    const bool text=(bracesCount_+parens_==0&&prev().type!="bos"&&prev().type!="slash");
    push(PicoToken{text?"text":"dot",u".",u"\\."});return true;
  }
  if(value==u'?'){
    const bool group=prev().value==u"(";
    if(!group&&peek()==u'('&&peek(2)!=u'?'){
      extglobOpen("qmark",value);return true;
    }
    if(prev().type=="paren"){
      const auto next=peek();std::u16string output=string;
      if(prev().value==u"("&&next!=u'!'&&next!=u'='&&next!=u'<'&&next!=u':')
        output=u"\\?";
      push(PicoToken{"text",string,output});return true;
    }
    push(PicoToken{"qmark",string,u"[^/]"});return true;
  }
  if(value==u'!'){
    if(peek()==u'('&&
      (peek(2)!=u'?'||(peek(3)!=u'!'&&peek(3)!=u'='&&peek(3)!=u'<'&&peek(3)!=u':'))){
      extglobOpen("negate",value);return true;
    }
    if(index_==0){
      int count=1;
      while(peek()==u'!'&&(peek(2)!=u'('||peek(3)==u'?')){
        advance();start_++;count++;
      }
      if(count%2){negated_=true;start_++;}
      return true;
    }
  }
  if(value==u'+'){
    if(peek()==u'('&&peek(2)!=u'?'){
      extglobOpen("plus",value);return true;
    }
    if(prev().value==u"(")push(PicoToken{"plus",string,u"\\+"});
    else if(prev().type=="bracket"||prev().type=="paren"||
      prev().type=="brace"||parens_>0)push(PicoToken{"plus",string});
    else push(PicoToken{"plus",u"\\+"});
    return true;
  }
  if(value==u'@'){
    if(peek()==u'('&&peek(2)!=u'?')
      push(PicoToken{"at",string,std::u16string(u""),{},0,false,false,true});
    else push(PicoToken{"text",string});
    return true;
  }
  std::u16string text=string;
  if(value==u'$'||value==u'^')text=u"\\"+text;
  while(peek()){
    const auto next=peek();
    if(next==u'@'||next==u'!'||next==u'['||next==u']'||next==u'.'||
      next==u','||next==u'$'||next==u'*'||next==u'+'||next==u'?'||
      next==u'^'||next==u'{'||next==u'}'||next==u'('||next==u')'||
      next==u'|'||next==u'\\'||next==u'/')break;
    text+=advance();
  }
  push(PicoToken{"text",text});return true;
}
} // namespace batbox::glob::detail
