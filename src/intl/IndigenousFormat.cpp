#include "intl/IndigenousInternal.hpp"
#include <unicode/locid.h>
#include <unicode/reldatefmt.h>
#include <unicode/unistr.h>
#include <map>
#include <cmath>
#include <stdexcept>
namespace batbox::intl {
std::shared_ptr<RelativeTimeFormat> indigenousRelativeTimeFormat(
    const std::string& locale,const std::string& style,const std::string& numeric){
  UDateRelativeDateTimeFormatterStyle selected;
  if(style=="long")selected=UDAT_STYLE_LONG;
  else if(style=="short")selected=UDAT_STYLE_SHORT;
  else if(style=="narrow")selected=UDAT_STYLE_NARROW;
  else throw std::invalid_argument("Invalid relative-time style");
  if(numeric!="always"&&numeric!="auto")throw std::invalid_argument("Invalid relative-time numeric option");
  UErrorCode status=U_ZERO_ERROR;
  auto formatter=std::make_shared<icu::RelativeDateTimeFormatter>(icu::Locale(locale.c_str()),nullptr,
    selected,UDISPCTX_CAPITALIZATION_NONE,status);
  if(U_FAILURE(status))throw std::runtime_error(u_errorName(status));
  auto result=std::make_shared<RelativeTimeFormat>();
  result->format=[formatter,numeric](double value,std::string unit){
    if(!std::isfinite(value))throw std::invalid_argument("Relative-time value must be finite");
    if(unit.ends_with('s'))unit.pop_back();
    static const std::map<std::string,URelativeDateTimeUnit> units={{"year",UDAT_REL_UNIT_YEAR},
      {"quarter",UDAT_REL_UNIT_QUARTER},{"month",UDAT_REL_UNIT_MONTH},{"week",UDAT_REL_UNIT_WEEK},
      {"day",UDAT_REL_UNIT_DAY},{"hour",UDAT_REL_UNIT_HOUR},{"minute",UDAT_REL_UNIT_MINUTE},
      {"second",UDAT_REL_UNIT_SECOND}};
    const auto found=units.find(unit);if(found==units.end())throw std::invalid_argument("Invalid relative-time unit");
    UErrorCode status=U_ZERO_ERROR;icu::UnicodeString output;
    if(numeric=="always")formatter->formatNumeric(value,found->second,output,status);
    else formatter->format(value,found->second,output,status);
    if(U_FAILURE(status))throw std::runtime_error(u_errorName(status));
    std::string text;output.toUTF8String(text);return text;
  };
  return result;
}
} // namespace batbox::intl
