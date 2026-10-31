#include "runtime/Unicode.hpp"
#include "runtime/JsonValue.hpp"
#include <unicode/unorm2.h>
#include <stdexcept>

namespace batbox::value {
std::string normalizeNFC(const std::string& value){
  const auto units=utf16CodeUnits(value);UErrorCode error=U_ZERO_ERROR;
  const auto* normalizer=unorm2_getNFCInstance(&error);
  if(U_FAILURE(error))throw std::runtime_error("NFC normalizer unavailable");
  const int32_t needed=unorm2_normalize(normalizer,
    reinterpret_cast<const UChar*>(units.data()),static_cast<int32_t>(units.size()),nullptr,0,&error);
  if(error!=U_BUFFER_OVERFLOW_ERROR&&U_FAILURE(error))throw std::runtime_error("NFC normalization failed");
  std::u16string output(static_cast<std::size_t>(needed),u'\0');error=U_ZERO_ERROR;
  const int32_t produced=unorm2_normalize(normalizer,
    reinterpret_cast<const UChar*>(units.data()),static_cast<int32_t>(units.size()),
    reinterpret_cast<UChar*>(output.data()),needed,&error);
  if(U_FAILURE(error))throw std::runtime_error("NFC normalization failed");
  output.resize(static_cast<std::size_t>(produced));return fromUtf16(output);
}
}
