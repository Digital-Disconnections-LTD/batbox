#include "flights/scanning/Scanning.hpp"
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace batbox::flights::scanning {
namespace {
constexpr std::string_view parentPrefix="{\"parentUuid\":";
constexpr std::string_view uuidKey="\"uuid\":\"";
constexpr std::string_view sidechainTrue="\"isSidechain\":true";
constexpr std::string_view timestampSuffix="\",\"timestamp\":\"";
constexpr std::size_t uuidLength=36;
struct MessageSpan {
  std::size_t start=0,end=0,parentStart=std::string::npos;
};
struct Range {std::size_t start=0,end=0;};

std::size_t pickDepthOneUuidCandidate(std::string_view buffer,std::size_t lineStart,
  const std::vector<std::size_t>& candidates){
  int depth=0;
  bool inString=false,escapeNext=false;
  std::size_t candidateIndex=0;
  for(std::size_t i=lineStart;candidateIndex<candidates.size();++i){
    if(i==candidates[candidateIndex]){
      if(depth==1&&!inString)return candidates[candidateIndex];
      ++candidateIndex;
    }
    const char byte=buffer[i];
    if(escapeNext)escapeNext=false;
    else if(inString){
      if(byte=='\\')escapeNext=true;
      else if(byte=='"')inString=false;
    }else if(byte=='"')inString=true;
    else if(byte=='{')++depth;
    else if(byte=='}')--depth;
  }
  return candidates.back();
}
}

std::string walkChainBeforeParse(const std::string& input){
  const std::string_view buffer(input);
  std::vector<MessageSpan> messages;
  std::vector<Range> metadata;
  std::unordered_map<std::string,std::size_t> uuidToSlot;
  std::size_t pos=0;
  while(pos<buffer.size()){
    const auto nl=buffer.find('\n',pos);
    const auto lineEnd=nl==std::string_view::npos?buffer.size():nl+1;
    if(lineEnd-pos>parentPrefix.size()&&buffer[pos]=='{'&&
       buffer.substr(pos,parentPrefix.size())==parentPrefix){
      const auto parentStart=buffer[pos+parentPrefix.size()]=='"'
        ?pos+parentPrefix.size()+1:std::string::npos;
      std::size_t firstAny=std::string::npos,suffix0=std::string::npos;
      std::vector<std::size_t> suffixN;
      std::size_t from=pos;
      while(true){
        const auto next=buffer.find(uuidKey,from);
        if(next==std::string_view::npos||next>=lineEnd)break;
        if(firstAny==std::string::npos)firstAny=next;
        const auto after=next+uuidKey.size()+uuidLength;
        if(after+timestampSuffix.size()<=lineEnd&&
           buffer.substr(after,timestampSuffix.size())==timestampSuffix){
          if(suffix0==std::string::npos)suffix0=next;
          else{
            if(suffixN.empty())suffixN.push_back(suffix0);
            suffixN.push_back(next);
          }
        }
        from=next+uuidKey.size();
      }
      const auto uuidKeyAt=!suffixN.empty()
        ?pickDepthOneUuidCandidate(buffer,pos,suffixN)
        :suffix0!=std::string::npos?suffix0:firstAny;
      if(uuidKeyAt!=std::string::npos){
        const auto uuidStart=uuidKeyAt+uuidKey.size();
        // UUID bytes are ASCII. substr also mirrors Buffer's clipped end at EOF.
        uuidToSlot[std::string(buffer.substr(uuidStart,uuidLength))]=messages.size();
        messages.push_back({pos,lineEnd,parentStart});
      }else metadata.push_back({pos,lineEnd});
    }else metadata.push_back({pos,lineEnd});
    pos=lineEnd;
  }

  std::size_t leafSlot=std::string::npos;
  for(std::size_t i=messages.size();i>0;--i){
    const auto& message=messages[i-1];
    const auto sidechain=buffer.find(sidechainTrue,message.start);
    if(sidechain==std::string_view::npos||sidechain>=message.end){
      leafSlot=i-1;break;
    }
  }
  if(leafSlot==std::string::npos)return input;

  std::unordered_set<std::size_t> seen,chain;
  std::size_t chainBytes=0;
  auto slot=leafSlot;
  while(true){
    if(!seen.insert(slot).second)break;
    const auto& message=messages[slot];
    chain.insert(message.start);
    chainBytes+=message.end-message.start;
    if(message.parentStart==std::string::npos)break;
    const std::string parent(buffer.substr(message.parentStart,uuidLength));
    const auto found=uuidToSlot.find(parent);
    if(found==uuidToSlot.end())break;
    slot=found->second;
  }
  // JS `len >> 1` first converts Buffer.length to signed Int32.
  const auto wrapped=static_cast<std::uint32_t>(buffer.size());
  const std::int64_t signedLength=wrapped<=0x7fffffffU
    ?static_cast<std::int64_t>(wrapped):static_cast<std::int64_t>(wrapped)-0x100000000LL;
  const auto half=signedLength>=0?signedLength/2:(signedLength-1)/2;
  if(static_cast<double>(buffer.size()-chainBytes)<static_cast<double>(half))return input;

  std::string output;
  output.reserve(chainBytes);
  std::size_t metaIndex=0;
  for(const auto& message:messages){
    while(metaIndex<metadata.size()&&metadata[metaIndex].start<message.start){
      const auto& range=metadata[metaIndex++];
      output+=buffer.substr(range.start,range.end-range.start);
    }
    if(chain.contains(message.start))output+=buffer.substr(message.start,message.end-message.start);
  }
  while(metaIndex<metadata.size()){
    const auto& range=metadata[metaIndex++];
    output+=buffer.substr(range.start,range.end-range.start);
  }
  return output;
}
} // namespace batbox::flights::scanning
