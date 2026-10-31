#include "terminal_output/BidiEmbeddingState.hpp"
#include "terminal_output/BidiTypes.hpp"
#include "runtime/JsonValue.hpp"

namespace batbox::terminal_output {
BidiEmbeddingState::BidiEmbeddingState(const std::string& text):
    units(value::utf16CodeUnits(text)),types(units.size()),levels(units.size()){
  for(std::size_t i=0;i<units.size();i++)types[i]=bidiCharType(units[i]);
}
int BidiEmbeddingState::count(std::uint32_t type)const{
  const auto found=typeCounts.find(type);
  return found==typeCounts.end()?0:found->second;
}
void BidiEmbeddingState::changeType(int index,std::uint32_t type){
  const auto old=types[index];types[index]=type;
  typeCounts[old]--;
  if(old&kNeutralIsolate)typeCounts[kNeutralIsolate]--;
  typeCounts[type]++;
  if(type&kNeutralIsolate)typeCounts[kNeutralIsolate]++;
}
int BidiEmbeddingState::matchingPDI(int isolateStart)const{
  int isolationLevel=1;
  for(int i=isolateStart+1;i<static_cast<int>(units.size());i++){
    const auto type=types[i];
    if(type&bidi_data::B)break;
    if(type&bidi_data::PDI){
      if(--isolationLevel==0)return i;
    }else if(type&kIsolateInit)isolationLevel++;
  }
  return -1;
}
int BidiEmbeddingState::autoLevel(int start,bool isFSI)const{
  for(int i=start;i<static_cast<int>(units.size());i++){
    const auto type=types[i];
    if(type&(bidi_data::R|bidi_data::AL))return 1;
    if((type&(bidi_data::B|bidi_data::L))||
        (isFSI&&type==bidi_data::PDI))return 0;
    if(type&kIsolateInit){
      const auto pdi=matchingPDI(i);
      i=pdi==-1?static_cast<int>(units.size()):pdi;
    }
  }
  return 0;
}
EmbeddingLevels BidiEmbeddingState::compute(){
  BidiParagraph* active=nullptr;
  for(int i=0;i<static_cast<int>(units.size());i++){
    if(!active){
      paragraphs.push_back({i,static_cast<int>(units.size())-1,
        autoLevel(i,false)});
      active=&paragraphs.back();
    }
    if(types[i]&bidi_data::B){active->end=i;active=nullptr;}
  }
  for(const auto& paragraph:paragraphs){
    typeCounts.clear();
    resolveExplicit(paragraph);
    const auto sequences=runSequences(paragraph);
    for(const auto& sequence:sequences){
      resolveWeak(sequence);
      resolveNeutral(sequence);
    }
    resolveImplicit(paragraph);
  }
  EmbeddingLevels output;
  output.reserve(levels.size());
  for(const auto level:levels)output.push_back(level);
  return output;
}
} // namespace batbox::terminal_output
