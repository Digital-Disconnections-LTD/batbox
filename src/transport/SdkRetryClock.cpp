#include "transport/SdkRetry.hpp"
#include <curl/curl.h>
#include <chrono>
#include <limits>
#include <random>

namespace batbox::transport {
SdkRetryClock indigenousSdkRetryClock(){
  return {[]{
    thread_local std::mt19937_64 engine(std::random_device{}());
    return std::generate_canonical<double,53>(engine);
  },[]{
    return std::chrono::duration<double,std::milli>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  },[](const std::string& text){
    const auto parsed=curl_getdate(text.c_str(),nullptr);
    return parsed==static_cast<time_t>(-1)?
      std::numeric_limits<double>::quiet_NaN():static_cast<double>(parsed)*1000;
  }};
}
} // namespace batbox::transport
