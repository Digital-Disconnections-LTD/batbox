#include "terminal_output/Bidi.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace batbox::terminal_output {
bool hasRTLCharacters(const std::string& text){
  for(const auto unit:value::utf16CodeUnits(text)){
    const auto cp=static_cast<std::uint32_t>(unit);
    if((cp>=0x0590&&cp<=0x05ff)||
        (cp>=0xfb1d&&cp<=0xfb4f)||
        (cp>=0x0600&&cp<=0x06ff)||
        (cp>=0x0750&&cp<=0x077f)||
        (cp>=0x08a0&&cp<=0x08ff)||
        (cp>=0xfb50&&cp<=0xfdff)||
        (cp>=0xfe70&&cp<=0xfeff)||
        (cp>=0x0780&&cp<=0x07bf)||
        (cp>=0x0700&&cp<=0x074f))return true;
  }
  return false;
}
BidiReorder::BidiReorder(std::function<BidiEnvironment()> environment,
    BidiFactory factory):environment_(std::move(environment)),
    factory_(std::move(factory)){}
bool BidiReorder::needsBidi(){
  if(!needsSoftwareBidi_){
    if(!environment_)throw std::invalid_argument("Bidi environment import required");
    const auto env=environment_();
    needsSoftwareBidi_=env.platform=="win32"||env.wtFlight.has_value()||
      env.termProgram=="vscode";
  }
  return *needsSoftwareBidi_;
}
LevelsProvider& BidiReorder::getBidi(){
  if(!bidi_){
    if(!factory_)throw std::invalid_argument("Bidi factory import required");
    bidi_=factory_();
  }
  return bidi_;
}
ClusteredCharsRef BidiReorder::reorder(
    const ClusteredCharsRef& characters){
  if(!characters)throw std::invalid_argument("Clustered characters required");
  if(!needsBidi()||characters->empty())return characters;
  std::string plainText;
  for(const auto& character:*characters)plainText+=character->value;
  if(!hasRTLCharacters(plainText))return characters;
  const auto levels=getBidi()(plainText);
  std::vector<double> charLevels;
  charLevels.reserve(characters->size());
  std::size_t offset=0;
  for(const auto& character:*characters){
    charLevels.push_back(offset<levels.size()?levels[offset]:
      std::numeric_limits<double>::quiet_NaN());
    offset+=value::stringLength(character->value);
  }
  auto reordered=std::make_shared<ClusteredChars>(*characters);
  double maxLevel=-std::numeric_limits<double>::infinity();
  for(const auto value:charLevels){
    if(std::isnan(value)){maxLevel=value;break;}
    if(value>maxLevel)maxLevel=value;
  }
  for(auto level=maxLevel;level>=1;level--){
    std::size_t i=0;
    while(i<reordered->size()){
      if(charLevels[i]>=level){
        auto j=i+1;
        while(j<reordered->size()&&charLevels[j]>=level)j++;
        std::reverse(reordered->begin()+i,reordered->begin()+j);
        std::reverse(charLevels.begin()+i,charLevels.begin()+j);
        i=j;
      }else i++;
    }
  }
  return reordered;
}
} // namespace batbox::terminal_output
