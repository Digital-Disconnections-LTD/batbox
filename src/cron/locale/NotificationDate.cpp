#include "cron/locale/NotificationDate.hpp"
#include <unicode/dtptngen.h>
#include <unicode/locid.h>
#include <unicode/smpdtfmt.h>
#include <unicode/timezone.h>
#include <cmath>
#include <memory>
#include <stdexcept>

namespace batbox::cron::locale {
std::string formatLocalDateTime(double epochMs,
    const std::optional<std::string>& locale,
    const std::optional<std::string>& timeZone){
  if(!std::isfinite(epochMs)||std::abs(epochMs)>8.64e15)
    return "Invalid Date";
  UErrorCode status=U_ZERO_ERROR;
  icu::Locale selected=icu::Locale::getDefault();
  if(locale){
    selected=icu::Locale::forLanguageTag(*locale,status);
    if(U_FAILURE(status))throw std::invalid_argument("Invalid date locale");
  }
  std::unique_ptr<icu::DateTimePatternGenerator> patterns(
    icu::DateTimePatternGenerator::createInstance(selected,status));
  if(U_FAILURE(status)||!patterns)throw std::runtime_error(
    "Native date pattern unavailable");
  const auto pattern=patterns->getBestPattern(
    icu::UnicodeString("yMdjms"),status);
  if(U_FAILURE(status))throw std::runtime_error("Native date pattern failed");
  icu::SimpleDateFormat formatter(pattern,selected,status);
  if(U_FAILURE(status))throw std::runtime_error("Native date formatter failed");
  if(timeZone){
    icu::UnicodeString id=icu::UnicodeString::fromUTF8(*timeZone);
    formatter.adoptTimeZone(icu::TimeZone::createTimeZone(id));
  }else formatter.adoptTimeZone(icu::TimeZone::createDefault());
  icu::UnicodeString result;
  formatter.format(std::trunc(epochMs),result);
  std::string text;result.toUTF8String(text);
  std::string normalized;
  for(std::size_t i=0;i<text.size();++i){
    if(i+2<text.size()&&text.compare(i,3,"\xE2\x80\xAF")==0){
      normalized+=' ';i+=2;
    }else normalized+=text[i];
  }
  return normalized;
}
std::function<std::string(double)> indigenousNotificationDateFormatter(){
  return [](double epochMs){return formatLocalDateTime(epochMs);};
}
}
