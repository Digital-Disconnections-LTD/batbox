#include "terminal_output/BidiEmbeddingState.hpp"
#include "terminal_output/BidiTypes.hpp"
#include <algorithm>

namespace batbox::terminal_output {
void BidiEmbeddingState::resolveNeutral(const BidiRunSequence& sequence){
  if(!count(kNeutralIsolate))return;
  const auto& indices=sequence.indices;
  const auto sos=sequence.sos,eos=sequence.eos;
  const auto embedDirection=(levels[indices.front()]&1)?
    bidi_data::R:bidi_data::L;
  constexpr auto rightTypes=bidi_data::R|bidi_data::EN|bidi_data::AN;
  constexpr auto strongTypes=rightTypes|bidi_data::L;
  std::vector<std::pair<int,int>> bracketPairs;
  struct Opener {std::uint32_t character;int sequenceIndex;};
  std::vector<Opener> openerStack;
  for(int si=0;si<static_cast<int>(indices.size());si++){
    if(types[indices[si]]&kNeutralIsolate){
      const auto character=static_cast<std::uint32_t>(units[indices[si]]);
      if(openingToClosingBracket(character)){
        if(openerStack.size()<63)openerStack.push_back({character,si});
        else break;
      }else if(const auto opposite=closingToOpeningBracket(character)){
        for(auto stackIndex=static_cast<int>(openerStack.size())-1;
            stackIndex>=0;stackIndex--){
          const auto stackChar=openerStack[stackIndex].character;
          if(stackChar==*opposite||
              closingToOpeningBracket(canonicalBracket(character))==
                std::optional<std::uint32_t>{stackChar}||
              openingToClosingBracket(canonicalBracket(stackChar))==
                std::optional<std::uint32_t>{character}){
            bracketPairs.push_back({openerStack[stackIndex].sequenceIndex,si});
            openerStack.resize(stackIndex);
            break;
          }
        }
      }
    }
  }
  std::sort(bracketPairs.begin(),bracketPairs.end(),
    [](const auto& a,const auto& b){return a.first<b.first;});
  for(const auto [openIndex,closeIndex]:bracketPairs){
    bool foundStrong=false;
    std::uint32_t useStrong=0;
    for(auto si=openIndex+1;si<closeIndex;si++){
      const auto i=indices[si];
      if(types[i]&strongTypes){
        foundStrong=true;
        const auto lr=types[i]&rightTypes?bidi_data::R:bidi_data::L;
        if(lr==embedDirection){useStrong=lr;break;}
      }
    }
    if(foundStrong&&!useStrong){
      useStrong=sos;
      for(auto si=openIndex-1;si>=0;si--){
        const auto i=indices[si];
        if(types[i]&strongTypes){
          const auto lr=types[i]&rightTypes?bidi_data::R:bidi_data::L;
          useStrong=lr!=embedDirection?lr:embedDirection;
          break;
        }
      }
    }
    if(useStrong){
      types[indices[openIndex]]=types[indices[closeIndex]]=useStrong;
      if(useStrong!=embedDirection){
        for(auto si=openIndex+1;si<static_cast<int>(indices.size());si++){
          if(!(types[indices[si]]&kBnLike)){
            if(bidiCharType(units[indices[si]])&bidi_data::NSM)
              types[indices[si]]=useStrong;
            break;
          }
        }
      }
      if(useStrong!=embedDirection){
        for(auto si=closeIndex+1;si<static_cast<int>(indices.size());si++){
          if(!(types[indices[si]]&kBnLike)){
            if(bidiCharType(units[indices[si]])&bidi_data::NSM)
              types[indices[si]]=useStrong;
            break;
          }
        }
      }
    }
  }
  for(int si=0;si<static_cast<int>(indices.size());si++){
    if(types[indices[si]]&kNeutralIsolate){
      auto start=si,end=si;
      auto previous=sos;
      for(auto si2=si-1;si2>=0;si2--){
        if(types[indices[si2]]&kBnLike)start=si2;
        else{
          previous=types[indices[si2]]&rightTypes?
            bidi_data::R:bidi_data::L;
          break;
        }
      }
      auto next=eos;
      for(auto si2=si+1;si2<static_cast<int>(indices.size());si2++){
        if(types[indices[si2]]&(kNeutralIsolate|kBnLike))end=si2;
        else{
          next=types[indices[si2]]&rightTypes?
            bidi_data::R:bidi_data::L;
          break;
        }
      }
      for(auto sj=start;sj<=end;sj++)
        types[indices[sj]]=previous==next?previous:embedDirection;
      si=end;
    }
  }
}
} // namespace batbox::terminal_output
