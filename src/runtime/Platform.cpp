#include "runtime/Platform.hpp"
#include "runtime/IndigenousTime.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <stdlib.h>
#else
#include <sys/random.h>
#endif

namespace batbox::platform {
namespace {
void entropy(void* output, std::size_t size) {
#if defined(_WIN32)
  auto* bytes=static_cast<unsigned char*>(output);
  while(size){
    const auto amount=static_cast<ULONG>(std::min<std::size_t>(size,
      std::numeric_limits<ULONG>::max()));
    if(BCryptGenRandom(nullptr,bytes,amount,BCRYPT_USE_SYSTEM_PREFERRED_RNG)!=0)
      throw std::runtime_error("Cannot obtain operating-system randomness");
    bytes+=amount;size-=amount;
  }
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
  ::arc4random_buf(output,size);
#else
  auto* bytes=static_cast<unsigned char*>(output);
  while (size) {
    const auto read=::getrandom(bytes,size,0);
    if (read<0 && errno==EINTR) continue;
    if (read<=0) throw std::runtime_error("Cannot obtain operating-system randomness");
    bytes+=read;size-=read;
  }
#endif
}
} // namespace
std::int64_t nowMillis() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
std::string isoTimestamp(double milliseconds) {
  if (!std::isfinite(milliseconds) || std::abs(milliseconds)>8640000000000000.0) throw std::range_error("Invalid time value");
  const auto integer=static_cast<std::int64_t>(milliseconds);
  auto seconds=integer/1000;
  auto remainder=integer%1000;
  if (remainder<0) {--seconds;remainder+=1000;}
  const auto time=static_cast<std::time_t>(seconds);
  std::tm utc{};
  if (!runtime::gmtimeUtc(time,utc)) throw std::range_error("Invalid time value");
  const int year=utc.tm_year+1900;
  std::ostringstream formatted;
  formatted<<std::setfill('0');
  if (year>=0 && year<=9999) formatted<<std::setw(4)<<year;
  else formatted<<(year<0?'-':'+')<<std::setw(6)<<std::abs(year);
  formatted<<'-'<<std::setw(2)<<utc.tm_mon+1<<'-'<<std::setw(2)<<utc.tm_mday
    <<'T'<<std::setw(2)<<utc.tm_hour<<':'<<std::setw(2)<<utc.tm_min<<':'<<std::setw(2)<<utc.tm_sec
    <<'.'<<std::setw(3)<<remainder<<'Z';
  return formatted.str();
}
std::string isoTimestamp() { return isoTimestamp(static_cast<double>(nowMillis())); }
std::string randomUUID() {
  std::array<unsigned char,16> bytes{};
  entropy(bytes.data(),bytes.size());
  bytes[6]=(bytes[6]&0x0f)|0x40;
  bytes[8]=(bytes[8]&0x3f)|0x80;
  constexpr char digits[]="0123456789abcdef";
  std::string result;
  for (std::size_t i=0;i<bytes.size();++i) {
    if (i==4||i==6||i==8||i==10) result+='-';
    result+=digits[bytes[i]>>4];result+=digits[bytes[i]&15];
  }
  return result;
}
double randomUnitInterval() {
  std::uint64_t value;
  entropy(&value,sizeof(value));
  return static_cast<double>(value>>11)/9007199254740992.0;
}
} // namespace batbox::platform
