#pragma once
#include <cstddef>
#include <string>
#include <string_view>

namespace batbox::bash_gadget {
class EndTruncatingAccumulator {
public:
  static constexpr double defaultMaxSize=1u<<25;
  explicit EndTruncatingAccumulator(double maxSize=defaultMaxSize):maxSize_(maxSize){}
  void append(std::string_view text);
  void appendBuffer(std::string_view utf8Bytes);
  std::string toString()const;
  void clear();
  std::size_t length()const;
  bool truncated()const{return isTruncated_;}
  std::size_t totalBytes()const{return totalBytesReceived_;}
private:
  std::string content_;
  double maxSize_;
  bool isTruncated_=false;
  std::size_t totalBytesReceived_=0;
};
}
