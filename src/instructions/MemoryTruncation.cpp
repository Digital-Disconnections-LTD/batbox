#include "instructions/MemoryPoetry.hpp"
#include "formatting/Format.hpp"
#include "runtime/JsonValue.hpp"

namespace batbox::instructions {
EntrypointTruncation truncateEntrypointContent(const std::string& raw){
  constexpr std::size_t maxLines=200,maxBytes=25000;
  const auto trimmed=value::trim(raw);
  std::vector<std::string> lines;
  for(std::size_t start=0;;){
    const auto next=trimmed.find('\n',start);
    lines.push_back(trimmed.substr(start,next==std::string::npos?next:next-start));
    if(next==std::string::npos)break;
    start=next+1;
  }
  EntrypointTruncation result;
  result.lineCount=lines.size();result.byteCount=value::stringLength(trimmed);
  result.wasLineTruncated=result.lineCount>maxLines;
  result.wasByteTruncated=result.byteCount>maxBytes;
  if(!result.wasLineTruncated&&!result.wasByteTruncated){result.content=trimmed;return result;}
  std::string cut=trimmed;
  if(result.wasLineTruncated){
    cut.clear();
    for(std::size_t i=0;i<maxLines;++i){if(i)cut+='\n';cut+=lines[i];}
  }
  if(value::stringLength(cut)>maxBytes){
    const auto units=value::utf16CodeUnits(cut);
    std::size_t lastNewline=std::string::npos;
    for(std::size_t i=0;i<=maxBytes&&i<units.size();++i)
      if(units[i]==u'\n')lastNewline=i;
    cut=value::slice(cut,0,lastNewline!=std::string::npos&&lastNewline>0
      ?static_cast<double>(lastNewline):static_cast<double>(maxBytes));
  }
  std::string reason;
  if(result.wasByteTruncated&&!result.wasLineTruncated)
    reason=formatting::formatFileSize(result.byteCount)+" (limit: "+
      formatting::formatFileSize(maxBytes)+") — index entries are too long";
  else if(result.wasLineTruncated&&!result.wasByteTruncated)
    reason=std::to_string(result.lineCount)+" lines (limit: 200)";
  else reason=std::to_string(result.lineCount)+" lines and "+
    formatting::formatFileSize(result.byteCount);
  result.content=cut+"\n\n> WARNING: MEMORY.md is "+reason+
    ". Only part of it was loaded. Keep index entries to one line under ~200 chars; move detail into topic files.";
  return result;
}
} // namespace batbox::instructions
