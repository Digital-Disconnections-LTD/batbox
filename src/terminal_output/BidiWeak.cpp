#include "terminal_output/BidiEmbeddingState.hpp"

namespace batbox::terminal_output {
void BidiEmbeddingState::resolveWeak(const BidiRunSequence& sequence){
  const auto& indices=sequence.indices;
  const auto sos=sequence.sos;
  if(count(bidi_data::NSM)){
    for(std::size_t si=0;si<indices.size();si++){
      const auto i=indices[si];
      if(types[i]&bidi_data::NSM){
        auto previous=sos;
        for(auto sj=static_cast<int>(si)-1;sj>=0;sj--){
          if(!(types[indices[sj]]&kBnLike)){
            previous=types[indices[sj]];break;
          }
        }
        changeType(i,previous&(kIsolateInit|bidi_data::PDI)?
          bidi_data::ON:previous);
      }
    }
  }
  if(count(bidi_data::EN)){
    for(std::size_t si=0;si<indices.size();si++){
      const auto i=indices[si];
      if(types[i]&bidi_data::EN){
        for(auto sj=static_cast<int>(si)-1;sj>=-1;sj--){
          const auto previous=sj==-1?sos:types[indices[sj]];
          if(previous&kStrong){
            if(previous==bidi_data::AL)changeType(i,bidi_data::AN);
            break;
          }
        }
      }
    }
  }
  if(count(bidi_data::AL)){
    for(const auto i:indices)
      if(types[i]&bidi_data::AL)changeType(i,bidi_data::R);
  }
  if(count(bidi_data::ES)||count(bidi_data::CS)){
    for(std::size_t si=1;si+1<indices.size();si++){
      const auto i=indices[si];
      if(types[i]&(bidi_data::ES|bidi_data::CS)){
        std::uint32_t previous=0,next=0;
        for(auto sj=static_cast<int>(si)-1;sj>=0;sj--){
          previous=types[indices[sj]];
          if(!(previous&kBnLike))break;
        }
        for(auto sj=si+1;sj<indices.size();sj++){
          next=types[indices[sj]];
          if(!(next&kBnLike))break;
        }
        if(previous==next&&
            (types[i]==bidi_data::ES?previous==bidi_data::EN:
              (previous&(bidi_data::EN|bidi_data::AN))!=0))
          changeType(i,previous);
      }
    }
  }
  if(count(bidi_data::EN)){
    for(std::size_t si=0;si<indices.size();si++){
      const auto i=indices[si];
      if(types[i]&bidi_data::EN){
        for(auto sj=static_cast<int>(si)-1;
            sj>=0&&(types[indices[sj]]&(bidi_data::ET|kBnLike));sj--)
          changeType(indices[sj],bidi_data::EN);
        for(si++;si<indices.size()&&
            (types[indices[si]]&(bidi_data::ET|kBnLike|
              bidi_data::EN));si++){
          if(types[indices[si]]!=bidi_data::EN)
            changeType(indices[si],bidi_data::EN);
        }
      }
    }
  }
  if(count(bidi_data::ET)||count(bidi_data::ES)||
      count(bidi_data::CS)){
    for(std::size_t si=0;si<indices.size();si++){
      const auto i=indices[si];
      if(types[i]&(bidi_data::ET|bidi_data::ES|bidi_data::CS)){
        changeType(i,bidi_data::ON);
        for(auto sj=static_cast<int>(si)-1;
            sj>=0&&(types[indices[sj]]&kBnLike);sj--)
          changeType(indices[sj],bidi_data::ON);
        for(auto sj=si+1;sj<indices.size()&&
            (types[indices[sj]]&kBnLike);sj++)
          changeType(indices[sj],bidi_data::ON);
      }
    }
  }
  if(count(bidi_data::EN)){
    auto previousStrong=sos;
    for(const auto i:indices){
      const auto type=types[i];
      if(type&bidi_data::EN){
        if(previousStrong==bidi_data::L)
          changeType(i,bidi_data::L);
      }else if(type&kStrong)previousStrong=type;
    }
  }
}
} // namespace batbox::terminal_output
