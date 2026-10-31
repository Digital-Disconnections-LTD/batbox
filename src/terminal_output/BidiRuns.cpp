#include "terminal_output/BidiEmbeddingState.hpp"
#include <algorithm>

namespace batbox::terminal_output {
std::vector<BidiRunSequence> BidiEmbeddingState::runSequences(
    const BidiParagraph& paragraph){
  std::vector<BidiLevelRun> runs;
  int isolationLevel=0;
  for(int i=paragraph.start;i<=paragraph.end;i++){
    const auto type=types[i];
    if(!(type&kBnLike)){
      const auto level=levels[i];
      const bool isolate=type&kIsolateInit;
      const bool pdi=type==bidi_data::PDI;
      if(isolate)isolationLevel++;
      if(!runs.empty()&&level==runs.back().level){
        runs.back().end=i;
        runs.back().endsWithIsolate=isolate;
      }else{
        runs.push_back({i,i,level,pdi,isolate});
      }
      if(pdi)isolationLevel--;
    }
  }
  std::vector<BidiRunSequence> sequences;
  for(std::size_t runIndex=0;runIndex<runs.size();runIndex++){
    const auto& run=runs[runIndex];
    if(!run.startsWithPDI||
        (run.startsWithPDI&&isolationPairs.find(run.start)==
          isolationPairs.end())){
      std::vector<const BidiLevelRun*> sequenceRuns{&run};
      auto* current=&run;
      while(current&&current->endsWithIsolate){
        const auto found=isolationPairs.find(current->end);
        if(found==isolationPairs.end())break;
        const auto pdiIndex=found->second;
        bool advanced=false;
        for(auto i=runIndex+1;i<runs.size();i++){
          if(runs[i].start==pdiIndex){
            current=&runs[i];sequenceRuns.push_back(current);
            advanced=true;break;
          }
        }
        if(!advanced)break;
      }
      BidiRunSequence sequence;
      for(const auto* item:sequenceRuns)
        for(auto j=item->start;j<=item->end;j++)
          sequence.indices.push_back(j);
      const auto firstLevel=levels[sequence.indices.front()];
      int previousLevel=paragraph.level;
      for(auto i=sequence.indices.front()-1;i>=0;i--){
        if(!(types[i]&kBnLike)){
          previousLevel=levels[i];break;
        }
      }
      const auto lastIndex=sequence.indices.back();
      const auto lastLevel=levels[lastIndex];
      int nextLevel=paragraph.level;
      if(!(types[lastIndex]&kIsolateInit)){
        for(auto i=lastIndex+1;i<=paragraph.end;i++){
          if(!(types[i]&kBnLike)){
            nextLevel=levels[i];break;
          }
        }
      }
      sequence.sos=std::max(previousLevel,static_cast<int>(firstLevel))%2?
        bidi_data::R:bidi_data::L;
      sequence.eos=std::max(nextLevel,static_cast<int>(lastLevel))%2?
        bidi_data::R:bidi_data::L;
      sequences.push_back(std::move(sequence));
    }
  }
  (void)isolationLevel;
  return sequences;
}
} // namespace batbox::terminal_output
