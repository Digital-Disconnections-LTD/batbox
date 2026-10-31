#include "terminal_output/LineRender.hpp"
#include "runtime/JsonValue.hpp"
#include <cmath>
#include <stdexcept>

namespace batbox::terminal_output {
namespace {
std::optional<std::uint32_t> codePointAtZero(const std::string& text){
  const auto units=value::utf16CodeUnits(text);
  if(units.empty())return {};
  const auto first=static_cast<std::uint32_t>(units[0]);
  if(first>=0xd800&&first<=0xdbff&&units.size()>1){
    const auto second=static_cast<std::uint32_t>(units[1]);
    if(second>=0xdc00&&second<=0xdfff)
      return 0x10000+((first-0xd800)<<10)+(second-0xdc00);
  }
  return first;
}
}
double writeLineToScreen(terminal_screen::Screen& screen,
    const std::string& line,double x,double y,double screenWidth,
    StylePool& stylePool,CharStash& charStash,const LineServices& services){
  auto stashed=charStash.find(line);
  ClusteredCharsRef characters;
  if(stashed==charStash.end()){
    if(!services.tokenize||!services.styledCharsFromTokens||
        !services.reorderBidi)
      throw std::invalid_argument("Line tokenizer and bidi imports required");
    characters=services.reorderBidi(styledCharsWithGraphemeClustering(
      services.styledCharsFromTokens(services.tokenize(line)),
      stylePool,services));
    charStash.emplace(line,characters);
  }else characters=stashed->second;
  double offsetX=x;
  for(std::size_t charIdx=0;charIdx<characters->size();charIdx++){
    const auto& character=*characters->at(charIdx);
    const auto codePoint=codePointAtZero(character.value);
    if(codePoint&&*codePoint<=0x1f){
      if(*codePoint==0x09){
        const double tabWidth=8;
        const double spacesToNextStop=tabWidth-std::fmod(offsetX,tabWidth);
        for(int i=0;i<spacesToNextStop&&offsetX<screenWidth;i++){
          terminal_screen::setCellAt(screen,offsetX,y,terminal_screen::Cell{
            " ",stylePool.none(),terminal_screen::CellWidth::Narrow,{}});
          offsetX++;
        }
      }else if(*codePoint==0x1b){
        const auto nextChar=charIdx+1<characters->size()?
          std::optional<std::string>{characters->at(charIdx+1)->value}:
          std::optional<std::string>{};
        const auto nextCode=nextChar?codePointAtZero(*nextChar):
          std::optional<std::uint32_t>{};
        if(nextChar&&(*nextChar=="("||*nextChar==")"||
            *nextChar=="*"||*nextChar=="+")){
          charIdx+=2;
        }else if(nextChar&&*nextChar=="["){
          charIdx++;
          while(charIdx+1<characters->size()){
            charIdx++;
            const auto c=codePointAtZero(characters->at(charIdx)->value);
            if(c&&*c>=0x40&&*c<=0x7e)break;
          }
        }else if(nextChar&&(*nextChar=="]"||*nextChar=="P"||
            *nextChar=="_"||*nextChar=="^"||*nextChar=="X")){
          charIdx++;
          while(charIdx+1<characters->size()){
            charIdx++;
            const auto& c=characters->at(charIdx)->value;
            if(c=="\x07")break;
            if(c=="\x1b"&&charIdx+1<characters->size()&&
                characters->at(charIdx+1)->value=="\\"){
              charIdx++;break;
            }
          }
        }else if(nextCode&&*nextCode>=0x30&&*nextCode<=0x7e){
          charIdx++;
        }
      }
      continue;
    }
    const auto charWidth=character.width;
    if(charWidth==0)continue;
    const bool isWideCharacter=charWidth>=2;
    if(isWideCharacter&&offsetX+2>screenWidth){
      terminal_screen::setCellAt(screen,offsetX,y,terminal_screen::Cell{
        " ",stylePool.none(),terminal_screen::CellWidth::SpacerHead,{}});
      offsetX++;
      continue;
    }
    terminal_screen::setCellAt(screen,offsetX,y,terminal_screen::Cell{
      character.value,character.styleId,
      isWideCharacter?terminal_screen::CellWidth::Wide:
        terminal_screen::CellWidth::Narrow,character.hyperlink});
    offsetX+=isWideCharacter?2:1;
  }
  return offsetX;
}
} // namespace batbox::terminal_output
