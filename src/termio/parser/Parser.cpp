#include "termio/parser/Parser.hpp"

namespace batbox::termio::parser {
namespace {
std::vector<std::u16string> codepoints(std::u16string_view text){
  std::vector<std::u16string> result;
  for(std::size_t i=0;i<text.size();++i){
    if(text[i]>=0xd800&&text[i]<=0xdbff&&i+1<text.size()&&
       text[i+1]>=0xdc00&&text[i+1]<=0xdfff)result.emplace_back(text.substr(i++,2));
    else result.emplace_back(text.substr(i,1));
  }
  return result;
}
}
void Parser::reset(){
  tokenizer_.reset();style=defaultStyle();inLink=false;linkUrl.reset();
}
std::vector<Action> Parser::feed(const std::string& input){
  const auto tokens=tokenizer_.feed(input);
  std::vector<Action> actions;
  for(const auto& token:tokens){
    auto tokenActions=processToken(token);
    actions.insert(actions.end(),std::make_move_iterator(tokenActions.begin()),
      std::make_move_iterator(tokenActions.end()));
  }
  return actions;
}
std::vector<Action> Parser::processToken(const termio::Token& token){
  if(token.type=="text")return processText(token.value);
  if(token.type=="sequence")return processSequence(token.value);
  return {};
}
std::vector<Action> Parser::processText(const std::string& text){
  std::vector<Action> actions;std::u16string current;
  const auto units=value::utf16CodeUnits(text);
  for(const auto& character:codepoints(units)){
    if(character[0]==0x07){
      if(!current.empty()){
        auto graphemes=segmentGraphemes(value::fromUtf16(current));
        if(!graphemes.empty())actions.emplace_back(TextAction{std::move(graphemes),style});
        current.clear();
      }
      actions.emplace_back(BellAction{});
    }else current+=character;
  }
  if(!current.empty()){
    auto graphemes=segmentGraphemes(value::fromUtf16(current));
    if(!graphemes.empty())actions.emplace_back(TextAction{std::move(graphemes),style});
  }
  return actions;
}
std::vector<Action> Parser::processSequence(const std::string& sequence){
  const auto type=identifySequence(sequence);
  if(type=="csi"){
    auto action=parseCSI(sequence);
    if(!action)return {};
    if(const auto sgr=std::get_if<SgrAction>(&*action)){
      style=applySGR(sgr->params,style);return {};
    }
    return {std::move(*action)};
  }
  if(type=="osc"){
    auto units=value::utf16CodeUnits(sequence);
    std::u16string content=units.size()>=2?units.substr(2):std::u16string{};
    if(!content.empty()&&content.back()==0x07)content.pop_back();
    else if(content.size()>=2&&content.substr(content.size()-2)==u"\x1b\\")
      content.resize(content.size()-2);
    auto action=parseOSC(value::fromUtf16(content));
    if(!action)return {};
    if(const auto link=std::get_if<LinkAction>(&*action)){
      if(link->type=="start"){
        inLink=true;linkUrl=link->url;
      }else{inLink=false;linkUrl.reset();}
    }
    return {std::move(*action)};
  }
  if(type=="esc"){
    const auto units=value::utf16CodeUnits(sequence);
    auto action=parseEsc(value::fromUtf16(std::u16string_view(units).substr(1)));
    return action?std::vector<Action>{std::move(*action)}:std::vector<Action>{};
  }
  return {UnknownAction{sequence}};
}
}
