#include "terminal_output/BidiEmbeddingState.hpp"
#include <vector>

namespace batbox::terminal_output {
namespace {
struct Status {int level=0;std::uint32_t overrideType=0;bool isolate=false;
  std::optional<int> isolateIndex;};
int nextEven(int value){return value+((value&1)?1:2);}
int nextOdd(int value){return value+((value&1)?2:1);}
}
void BidiEmbeddingState::resolveExplicit(const BidiParagraph& paragraph){
  constexpr int maxDepth=125;
  constexpr auto formatting=bidi_data::RLE|bidi_data::LRE|
    bidi_data::RLO|bidi_data::LRO|kIsolateInit|
    bidi_data::PDI|bidi_data::PDF|bidi_data::B;
  std::vector<Status> stack{{paragraph.level,0,false,{}}};
  int overflowIsolate=0,overflowEmbedding=0,validIsolate=0;
  for(int i=paragraph.start;i<=paragraph.end;i++){
    auto type=types[i];
    auto top=stack.back();
    typeCounts[type]++;
    if(type&kNeutralIsolate)typeCounts[kNeutralIsolate]++;
    if(type&formatting){
      if(type&(bidi_data::RLE|bidi_data::LRE)){
        levels[i]=top.level;
        const auto level=type==bidi_data::RLE?
          nextOdd(top.level):nextEven(top.level);
        if(level<=maxDepth&&!overflowIsolate&&!overflowEmbedding)
          stack.push_back({level,0,false,{}});
        else if(!overflowIsolate)overflowEmbedding++;
      }else if(type&(bidi_data::RLO|bidi_data::LRO)){
        levels[i]=top.level;
        const auto level=type==bidi_data::RLO?
          nextOdd(top.level):nextEven(top.level);
        if(level<=maxDepth&&!overflowIsolate&&!overflowEmbedding)
          stack.push_back({level,type&bidi_data::RLO?
            bidi_data::R:bidi_data::L,false,{}});
        else if(!overflowIsolate)overflowEmbedding++;
      }else if(type&kIsolateInit){
        if(type&bidi_data::FSI)
          type=autoLevel(i+1,true)==1?bidi_data::RLI:bidi_data::LRI;
        levels[i]=top.level;
        if(top.overrideType)changeType(i,top.overrideType);
        const auto level=type==bidi_data::RLI?
          nextOdd(top.level):nextEven(top.level);
        if(level<=maxDepth&&overflowIsolate==0&&overflowEmbedding==0){
          validIsolate++;
          stack.push_back({level,0,true,i});
        }else overflowIsolate++;
      }else if(type&bidi_data::PDI){
        if(overflowIsolate>0)overflowIsolate--;
        else if(validIsolate>0){
          overflowEmbedding=0;
          while(!stack.back().isolate)stack.pop_back();
          const auto isolateIndex=stack.back().isolateIndex;
          if(isolateIndex){
            isolationPairs[*isolateIndex]=i;
            isolationPairs[i]=*isolateIndex;
          }
          stack.pop_back();validIsolate--;
        }
        top=stack.back();levels[i]=top.level;
        if(top.overrideType)changeType(i,top.overrideType);
      }else if(type&bidi_data::PDF){
        if(overflowIsolate==0){
          if(overflowEmbedding>0)overflowEmbedding--;
          else if(!top.isolate&&stack.size()>1){
            stack.pop_back();top=stack.back();
          }
        }
        levels[i]=top.level;
      }else if(type&bidi_data::B)levels[i]=paragraph.level;
    }else{
      levels[i]=top.level;
      if(top.overrideType&&type!=bidi_data::BN)
        changeType(i,top.overrideType);
    }
  }
}
} // namespace batbox::terminal_output
