#pragma once
#include "terminal_output/LineRender.hpp"
#include <functional>

namespace batbox::terminal_output {
struct OutputRenderServices {
  LineServices line;
  std::function<double(const std::string&)> widestLine;
  std::function<std::string(const std::string&,double,double)> sliceAnsi;
  std::function<void(const std::string&)> logForDebugging;
};
void applyWriteOperation(Output& output,const WriteOperation& operation,
  const ClipRef& clip,const OutputRenderServices& services,
  double& writeCells);
OutputRenderServices makeIndigenousOutputRenderServices(LineServices line,
  std::function<std::string(const std::string&,double,double)> sliceAnsi,
  std::function<void(const std::string&)> logForDebugging);
OutputRenderServices makeIndigenousOutputRenderServices(LineServices line,
  std::function<void(const std::string&)> logForDebugging);
} // namespace batbox::terminal_output
