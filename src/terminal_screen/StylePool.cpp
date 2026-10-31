#include "terminal_screen/StylePool.hpp"
#include "ansi_tokenize/AnsiTokenize.hpp"
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace batbox::terminal_screen {
namespace {
using ansi_tokenize::AnsiCode;
using ansi_tokenize::CodeRef;
using ansi_tokenize::Codes;
const auto INVERSE_CODE=std::make_shared<AnsiCode>(AnsiCode{
  "ansi","\x1b[7m","\x1b[27m"});
const auto BOLD_CODE=std::make_shared<AnsiCode>(AnsiCode{
  "ansi","\x1b[1m","\x1b[22m"});
const auto UNDERLINE_CODE=std::make_shared<AnsiCode>(AnsiCode{
  "ansi","\x1b[4m","\x1b[24m"});
const auto YELLOW_FG_CODE=std::make_shared<AnsiCode>(AnsiCode{
  "ansi","\x1b[33m","\x1b[39m"});
bool hasEndCode(const Codes& codes,const std::string& end){
  return std::any_of(codes.begin(),codes.end(),[&](const auto& code){
    return code->endCode==end;});
}
std::string styleKey(const Codes& codes){
  if(codes.empty())return "";
  std::string key;
  for(std::size_t i=0;i<codes.size();++i){
    if(i)key.push_back('\0');
    key+=codes[i]->code;
  }
  return key;
}
}
bool hasVisibleSpaceEffect(const ansi_tokenize::Codes& styles){
  for(const auto& style:styles){
    const auto& end=style->endCode;
    if(end=="\x1b[49m"||end=="\x1b[27m"||end=="\x1b[24m"||
        end=="\x1b[29m"||end=="\x1b[55m")return true;
  }
  return false;
}
StylePool::StylePool(){none_=intern(std::make_shared<Codes>());}
std::int32_t StylePool::intern(StylesRef styles){
  if(!styles)throw std::invalid_argument("Style array required");
  const auto key=styleKey(*styles);
  if(const auto existing=ids_.find(key);existing!=ids_.end())
    return existing->second;
  const auto rawId=static_cast<std::uint32_t>(styles_.size());
  styles_.push_back(styles->empty()?std::make_shared<Codes>():styles);
  const auto encoded=(rawId<<1)|
    (!styles->empty()&&hasVisibleSpaceEffect(*styles)?1u:0u);
  const auto id=std::bit_cast<std::int32_t>(encoded);
  ids_.emplace(key,id);
  return id;
}
StylesRef StylePool::get(std::int32_t id)const{
  const auto index=static_cast<std::uint32_t>(id)>>1;
  if(index>=styles_.size())return std::make_shared<Codes>();
  return styles_[index];
}
std::string StylePool::transition(std::int32_t fromId,
    std::int32_t toId){
  if(fromId==toId)return "";
  const double key=static_cast<double>(fromId)*0x100000+toId;
  if(const auto existing=transitionStash_.find(key);
      existing!=transitionStash_.end())return existing->second;
  const auto value=ansi_tokenize::ansiCodesToString(
    ansi_tokenize::diffAnsiCodes(*get(fromId),*get(toId)));
  transitionStash_.emplace(key,value);
  return value;
}
std::int32_t StylePool::withInverse(std::int32_t baseId){
  if(const auto stashed=inverseStash_.find(baseId);
      stashed!=inverseStash_.end())return stashed->second;
  const auto base=get(baseId);
  const auto id=hasEndCode(*base,"\x1b[27m")?baseId:[&]{
    auto next=std::make_shared<Codes>(*base);
    next->push_back(INVERSE_CODE);
    return intern(next);}();
  inverseStash_[baseId]=id;
  return id;
}
std::int32_t StylePool::withCurrentMatch(std::int32_t baseId){
  if(const auto stashed=currentMatchStash_.find(baseId);
      stashed!=currentMatchStash_.end())return stashed->second;
  const auto base=get(baseId);
  auto codes=std::make_shared<Codes>();
  for(const auto& code:*base)
    if(code->endCode!="\x1b[39m"&&code->endCode!="\x1b[49m")
      codes->push_back(code);
  codes->push_back(YELLOW_FG_CODE);
  if(!hasEndCode(*base,"\x1b[27m"))codes->push_back(INVERSE_CODE);
  if(!hasEndCode(*base,"\x1b[22m"))codes->push_back(BOLD_CODE);
  if(!hasEndCode(*base,"\x1b[24m"))codes->push_back(UNDERLINE_CODE);
  const auto id=intern(codes);
  currentMatchStash_[baseId]=id;
  return id;
}
void StylePool::setSelectionBg(CodeRef background){
  if((!selectionBgCode_&&!background)||
      (selectionBgCode_&&background&&
        selectionBgCode_->code==background->code))return;
  selectionBgCode_=std::move(background);
  selectionBgStash_.clear();
}
std::int32_t StylePool::withSelectionBg(std::int32_t baseId){
  const auto background=selectionBgCode_;
  if(!background)return withInverse(baseId);
  if(const auto stashed=selectionBgStash_.find(baseId);
      stashed!=selectionBgStash_.end())return stashed->second;
  auto kept=std::make_shared<Codes>();
  for(const auto& code:*get(baseId))
    if(code->endCode!="\x1b[49m"&&code->endCode!="\x1b[27m")
      kept->push_back(code);
  kept->push_back(background);
  const auto id=intern(kept);
  selectionBgStash_[baseId]=id;
  return id;
}
} // namespace batbox::terminal_screen
