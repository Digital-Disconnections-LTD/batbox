#include "terminal_output/BidiEmbeddingState.hpp"
#include "terminal_output/BidiTypes.hpp"

namespace batbox::terminal_output {
void BidiEmbeddingState::resolveImplicit(const BidiParagraph& paragraph){
  for(auto i=paragraph.start;i<=paragraph.end;i++){
    const auto level=levels[i];
    const auto type=types[i];
    if(level&1){
      if(type&(bidi_data::L|bidi_data::EN|bidi_data::AN))levels[i]++;
    }else{
      if(type&bidi_data::R)levels[i]++;
      else if(type&(bidi_data::AN|bidi_data::EN))levels[i]+=2;
    }
    if(type&kBnLike)
      levels[i]=i==0?paragraph.level:levels[i-1];
    if(i==paragraph.end||
        (bidiCharType(units[i])&(bidi_data::S|bidi_data::B))){
      for(auto j=i;j>=0&&
          (bidiCharType(units[j])&kTrailing);j--)
        levels[j]=paragraph.level;
    }
  }
}
} // namespace batbox::terminal_output
