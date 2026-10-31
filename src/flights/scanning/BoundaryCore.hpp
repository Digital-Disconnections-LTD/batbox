#pragma once
#include <cstddef>
#include <string>
#include <string_view>

namespace batbox::flights::scanning::detail {
struct LoadState {
  std::string out;
  std::size_t boundaryStartOffset=0,bufFileOff=0;
  bool hasPreservedSegment=false;
  std::string lastSnap,carry,straddleSnapCarry;
  bool hasLastSnap=false;
  std::size_t straddleSnapTailEnd=0;
};
struct ScanChunkResult {
  std::size_t lastSnapStart=std::string::npos,lastSnapEnd=std::string::npos,trailStart=0;
};
std::size_t processStraddle(LoadState& state,std::string_view chunk);
ScanChunkResult scanChunkLines(LoadState& state,std::string_view buffer,
  std::string_view boundaryMarker);
void captureSnap(LoadState& state,std::string_view buffer,std::string_view chunk,
  const ScanChunkResult& result);
void captureCarry(LoadState& state,std::string_view buffer,std::size_t trailStart);
void finalizeOutput(LoadState& state);
} // namespace batbox::flights::scanning::detail
