#include "terminal_output/OutputRender.hpp"
#include "text_width/AnsiSlice.hpp"
#include "text_width/Tabs.hpp"
#include <stdexcept>

namespace batbox::terminal_output {
OutputRenderServices makeIndigenousOutputRenderServices(LineServices line,
    std::function<std::string(const std::string&,double,double)> sliceAnsi,
    std::function<void(const std::string&)> logForDebugging){
  if(!line.stringWidth||!sliceAnsi||!logForDebugging)
    throw std::invalid_argument("Output width, ANSI slice, and debug imports required");
  OutputRenderServices result;
  result.line=std::move(line);
  result.widestLine=[width=result.line.stringWidth](const std::string& text){
    return text_width::widestLine(text,width);};
  result.sliceAnsi=std::move(sliceAnsi);
  result.logForDebugging=std::move(logForDebugging);
  return result;
}
OutputRenderServices makeIndigenousOutputRenderServices(LineServices line,
    std::function<void(const std::string&)> logForDebugging){
  if(!line.stringWidth)
    throw std::invalid_argument("Output string width import required");
  text_width::SliceServices slice;
  slice.tokenize=ansi_tokenize::tokenize;
  slice.reduceAnsiCodes=ansi_tokenize::reduceAnsiCodes;
  slice.undoAnsiCodes=ansi_tokenize::undoAnsiCodes;
  slice.ansiCodesToString=ansi_tokenize::ansiCodesToString;
  slice.stringWidth=[width=line.stringWidth](
      const std::optional<std::string>& value){
    return value?width(*value):0.0;};
  return makeIndigenousOutputRenderServices(std::move(line),
    [slice=std::move(slice)](const std::string& text,double from,double to){
      return text_width::sliceAnsi(text,from,to,slice);},
    std::move(logForDebugging));
}
} // namespace batbox::terminal_output
