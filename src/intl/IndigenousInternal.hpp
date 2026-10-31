#pragma once
#include "intl/Intl.hpp"
namespace batbox::intl {
std::shared_ptr<Segmenter> indigenousSegmenter(const std::string& granularity);
std::shared_ptr<RelativeTimeFormat> indigenousRelativeTimeFormat(
  const std::string& locale,const std::string& style,const std::string& numeric);
std::string indigenousTimeZone(),indigenousLocale();
std::optional<std::string> indigenousLocaleLanguage(const std::string& locale);
}
