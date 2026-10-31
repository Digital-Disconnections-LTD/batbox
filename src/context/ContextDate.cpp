#include "context/Context.hpp"
#include "runtime/IndigenousTime.hpp"
#include <chrono>
#include <cstdio>
#include <ctime>
#include <stdexcept>

namespace batbox::context {
std::string getLocalISODate(const std::optional<std::string>& overrideDate){
  if(overrideDate&&!overrideDate->empty())return *overrideDate;
  const auto now=std::chrono::system_clock::now();
  const auto value=std::chrono::system_clock::to_time_t(now);
  std::tm local{};
  if(!runtime::localtimeIndigenous(value,local))
    throw std::runtime_error("Local date conversion failed");
  char date[32];
  std::snprintf(date,sizeof(date),"%04d-%02d-%02d",local.tm_year+1900,
    local.tm_mon+1,local.tm_mday);
  return date;
}
}
