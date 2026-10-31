#include "intl/IndigenousInternal.hpp"
#include <unicode/locid.h>
#include <unicode/timezone.h>
#include <unicode/uloc.h>
#include <memory>
#include <stdexcept>
#include <vector>
namespace batbox::intl {
std::string indigenousTimeZone(){
  std::unique_ptr<icu::TimeZone> zone(icu::TimeZone::createDefault());
  icu::UnicodeString id;zone->getID(id);icu::UnicodeString canonical;
  UErrorCode status=U_ZERO_ERROR;UBool system=false;
  icu::TimeZone::getCanonicalID(id,canonical,system,status);
  if(U_FAILURE(status))throw std::runtime_error(u_errorName(status));
  std::string text;canonical.toUTF8String(text);
  if(text=="Etc/UTC"||text=="Etc/GMT")return "UTC";return text;
}
std::string indigenousLocale(){
  UErrorCode status=U_ZERO_ERROR;auto tag=icu::Locale::getDefault().toLanguageTag<std::string>(status);
  if(U_FAILURE(status))throw std::runtime_error(u_errorName(status));return tag;
}
std::optional<std::string> indigenousLocaleLanguage(const std::string& locale){
  UErrorCode status=U_ZERO_ERROR;int32_t parsed=0;std::vector<char> output(locale.size()*2+64);
  uloc_forLanguageTag(locale.c_str(),output.data(),static_cast<int32_t>(output.size()),&parsed,&status);
  if(U_FAILURE(status)||parsed!=static_cast<int32_t>(locale.size()))throw std::invalid_argument("Invalid locale identifier");
  const icu::Locale resolved(output.data());
  const std::string language=resolved.getLanguage();
  return language.empty()?std::nullopt:std::optional<std::string>(language);
}
Services indigenousServices(){return {indigenousSegmenter,indigenousRelativeTimeFormat,indigenousTimeZone,indigenousLocale,indigenousLocaleLanguage};}
} // namespace batbox::intl
