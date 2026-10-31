#include "cron/use_scheduled/FireTime.hpp"
#include "runtime/Platform.hpp"
#include <unicode/dtptngen.h>
#include <unicode/locid.h>
#include <unicode/smpdtfmt.h>
#include <unicode/timezone.h>
#include <cmath>
#include <memory>
#include <stdexcept>

namespace batbox::cron::use_scheduled {
std::string formatCronFireTime(double epochMs){
  if(!std::isfinite(epochMs)||std::abs(epochMs)>8.64e15)
    return "Invalid Date";
  UErrorCode status=U_ZERO_ERROR;
  const icu::Locale locale("en_US");
  std::unique_ptr<icu::DateTimePatternGenerator> patterns(
    icu::DateTimePatternGenerator::createInstance(locale,status));
  if(U_FAILURE(status)||!patterns)
    throw std::runtime_error("Cron fire-time pattern unavailable");
  const auto pattern=patterns->getBestPattern(
    icu::UnicodeString("MMMdjm"),status);
  if(U_FAILURE(status))
    throw std::runtime_error("Cron fire-time pattern failed");
  icu::SimpleDateFormat formatter(pattern,locale,status);
  if(U_FAILURE(status))
    throw std::runtime_error("Cron fire-time formatter failed");
  formatter.adoptTimeZone(icu::TimeZone::createDefault());
  icu::UnicodeString result;
  formatter.format(std::trunc(epochMs),result);
  std::string text;result.toUTF8String(text);
  std::string normalized;
  for(std::size_t i=0;i<text.size();++i){
    if(i+2<text.size()&&text.compare(i,3,"\xE2\x80\xAF")==0){
      normalized+=' ';i+=2;
    }else normalized+=text[i];
  }
  auto comma=normalized.find(", ");
  if(comma!=std::string::npos)normalized.replace(comma,2," ");
  else{
    const auto at=normalized.find(" at ");
    if(at!=std::string::npos)normalized.replace(at,4," ");
  }
  const auto am=normalized.find(" AM");
  const auto pm=normalized.find(" PM");
  const auto period=am!=std::string::npos?am:pm;
  if(period!=std::string::npos){
    normalized.erase(period,1);
    normalized[period]=static_cast<char>(
      normalized[period]-'A'+'a');
    normalized[period+1]='m';
  }
  return normalized;
}
std::function<std::string()> indigenousCronFireTimeFormatter(){
  return []{return formatCronFireTime(
    static_cast<double>(platform::nowMillis()));};
}
}
