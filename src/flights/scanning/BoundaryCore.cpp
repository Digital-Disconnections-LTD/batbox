#include "flights/scanning/BoundaryCore.hpp"
#include "runtime/JsonValue.hpp"

namespace batbox::flights::scanning::detail {
namespace {
constexpr std::string_view attrSnapPrefix="{\"type\":\"attribution-snapshot\"";
constexpr std::string_view systemPrefix="{\"type\":\"system\"";
constexpr char lf='\n';
constexpr std::string_view lfByte="\n";
constexpr std::size_t boundarySearchBound=256;
void sinkWrite(LoadState& state,std::string_view source,std::size_t start,std::size_t end){
  if(end>start)state.out+=source.substr(start,end-start);
}
bool hasPrefix(std::string_view source,std::string_view prefix,std::size_t at,std::size_t end){
  return end-at>=prefix.size()&&source.substr(at,prefix.size())==prefix;
}
// Parse only candidate lines. A marker inside user content must never become
// a transcript boundary.
std::optional<bool> parseBoundaryLine(std::string_view raw){
  try{
    const auto line=value::decodeUtf8(raw);
    const auto parsed=value::parse(line);
    if(value::property(parsed,"type")!="system"||
       value::property(parsed,"subtype")!="compact_boundary")return std::nullopt;
    return value::truthy(value::property(value::property(parsed,"compactMetadata"),"preservedSegment"));
  }catch(const std::exception&){return std::nullopt;}
}
}
std::size_t processStraddle(LoadState& state,std::string_view chunk){
  state.straddleSnapCarry.clear();state.straddleSnapTailEnd=0;
  if(state.carry.empty())return 0;
  const auto firstNl=chunk.find(lf);
  if(firstNl==std::string_view::npos)return 0;
  const auto tailEnd=firstNl+1;
  if(hasPrefix(state.carry,attrSnapPrefix,0,state.carry.size())){
    state.straddleSnapCarry=state.carry;
    state.straddleSnapTailEnd=tailEnd;
    state.lastSnap.clear();state.hasLastSnap=false;
  }else if(state.carry.size()<attrSnapPrefix.size()){
    return 0;
  }else{
    if(hasPrefix(state.carry,systemPrefix,0,state.carry.size())){
      std::string line=state.carry+std::string(chunk.substr(0,firstNl));
      const auto hit=parseBoundaryLine(line);
      if(hit){
        if(*hit)state.hasPreservedSegment=true;
        else{
          state.out.clear();state.boundaryStartOffset=state.bufFileOff;
          state.hasPreservedSegment=false;state.lastSnap.clear();state.hasLastSnap=false;
        }
      }
    }
    sinkWrite(state,state.carry,0,state.carry.size());
    sinkWrite(state,chunk,0,tailEnd);
  }
  state.bufFileOff+=state.carry.size()+tailEnd;
  state.carry.clear();
  return tailEnd;
}
ScanChunkResult scanChunkLines(LoadState& state,std::string_view buffer,
  std::string_view boundaryMarker){
  auto boundaryAt=buffer.find(boundaryMarker);
  std::size_t runStart=0,lineStart=0;
  ScanChunkResult result;
  auto nl=buffer.find(lf);
  while(nl!=std::string_view::npos){
    const auto lineEnd=nl+1;
    if(boundaryAt!=std::string_view::npos&&boundaryAt<lineStart)
      boundaryAt=buffer.find(boundaryMarker,lineStart);
    if(hasPrefix(buffer,attrSnapPrefix,lineStart,lineEnd)){
      sinkWrite(state,buffer,runStart,lineStart);
      result.lastSnapStart=lineStart;result.lastSnapEnd=lineEnd;
      runStart=lineEnd;
    }else if(boundaryAt!=std::string_view::npos&&boundaryAt>=lineStart&&
             boundaryAt<std::min(lineStart+boundarySearchBound,lineEnd)){
      const auto hit=parseBoundaryLine(buffer.substr(lineStart,nl-lineStart));
      if(hit){
        if(*hit)state.hasPreservedSegment=true;
        else{
          state.out.clear();state.boundaryStartOffset=state.bufFileOff+lineStart;
          state.hasPreservedSegment=false;state.lastSnap.clear();state.hasLastSnap=false;
          result.lastSnapStart=std::string::npos;
          state.straddleSnapCarry.clear();state.straddleSnapTailEnd=0;
          runStart=lineStart;
        }
      }
      boundaryAt=buffer.find(boundaryMarker,boundaryAt+boundaryMarker.size());
    }
    lineStart=lineEnd;nl=buffer.find(lf,lineStart);
  }
  sinkWrite(state,buffer,runStart,lineStart);
  result.trailStart=lineStart;
  return result;
}
void captureSnap(LoadState& state,std::string_view buffer,std::string_view chunk,
  const ScanChunkResult& result){
  if(result.lastSnapStart!=std::string::npos){
    state.lastSnap=buffer.substr(result.lastSnapStart,result.lastSnapEnd-result.lastSnapStart);
    state.hasLastSnap=true;
  }else if(!state.straddleSnapCarry.empty()){
    state.lastSnap=state.straddleSnapCarry;
    state.lastSnap+=chunk.substr(0,state.straddleSnapTailEnd);
    state.hasLastSnap=true;
  }
}
void captureCarry(LoadState& state,std::string_view buffer,std::size_t trailStart){
  state.carry=buffer.substr(trailStart);
}
void finalizeOutput(LoadState& state){
  if(!state.carry.empty()){
    if(hasPrefix(state.carry,attrSnapPrefix,0,state.carry.size())){
      state.lastSnap=state.carry;state.hasLastSnap=true;
    }else sinkWrite(state,state.carry,0,state.carry.size());
  }
  if(state.hasLastSnap){
    if(!state.out.empty()&&state.out.back()!=lf)sinkWrite(state,lfByte,0,1);
    sinkWrite(state,state.lastSnap,0,state.lastSnap.size());
  }
}
} // namespace batbox::flights::scanning::detail
