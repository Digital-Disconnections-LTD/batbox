#pragma once
#include "text_width/Layout.hpp"
#include <functional>
#include <string>
namespace batbox::text_width {
double widestLine(const std::string& text,
  const std::function<double(const std::string&)>& lineWidth);
std::string expandTabs(const std::string& text,double interval,
  const std::function<double(const std::string&)>& stringWidth);
inline std::string expandTabs(const std::string& text,
  const std::function<double(const std::string&)>& stringWidth){
  return expandTabs(text,8,stringWidth);
}
}
