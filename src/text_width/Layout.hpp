#pragma once
#include "text_width/StringWidth.hpp"
#include <functional>
#include <map>
#include <memory>
#include <string>
namespace batbox::text_width {
class LineWidthStash {
public:
  explicit LineWidthStash(std::function<double(const std::string&)> stringWidth);
  double lineWidth(const std::string& line);
private:
  std::function<double(const std::string&)> stringWidth_;
  std::map<std::string,double> stash_;
};
struct Measurement {double width=0,height=0;};
Measurement measureText(const std::string& text,double maxWidth,
  const std::function<double(const std::string&)>& lineWidth);
struct WrapServices {
  std::function<std::string(const std::string&,double,double)> sliceAnsi;
  std::function<double(const std::string&)> stringWidth;
  std::function<std::string(const std::string&,double,bool,bool)> wrapAnsi;
};
std::string sliceFit(const std::string& text,double start,double end,const WrapServices& services);
std::string truncate(const std::string& text,double columns,const std::string& position,const WrapServices& services);
std::string wrapText(const std::string& text,double maxWidth,const std::string& wrapType,const WrapServices& services);
std::shared_ptr<LineWidthStash> indigenousLineWidthStash(std::shared_ptr<StringWidth> width);
}
