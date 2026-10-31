#include "attachments/context/Sort.hpp"
#include "runtime/JsonValue.hpp"
#include <unicode/ucol.h>
#include <stdexcept>

namespace batbox::attachments::context {
bool utf16Less(const std::string& left,const std::string& right){
  return value::utf16CodeUnits(left)<value::utf16CodeUnits(right);
}
bool localeLess(const std::string& left,const std::string& right){
  UErrorCode error=U_ZERO_ERROR;
  // Batbox's implicit locale comparison uses en-US collation.
  UCollator* collator=ucol_open("en_US",&error);
  if(U_FAILURE(error)||!collator)
    throw std::runtime_error("Native locale collation unavailable");
  const auto order=ucol_strcollUTF8(collator,left.data(),
    static_cast<int32_t>(left.size()),right.data(),
    static_cast<int32_t>(right.size()),&error);
  ucol_close(collator);
  if(U_FAILURE(error))throw std::runtime_error("Native locale comparison failed");
  return order==UCOL_LESS;
}
} // namespace batbox::attachments::context
