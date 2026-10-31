#pragma once
#include <functional>
#include <map>
#include <mutex>
#include <string>

namespace batbox::settings::persistence {
class InternalWrites {
 public:
  using Clock=std::function<double()>;
  explicit InternalWrites(Clock now);
  void markInternalWrite(const std::string& path);
  bool consumeInternalWrite(const std::string& path,double windowMs);
  void clearInternalWrites();
 private:
  Clock now_;
  std::mutex mutex_;
  std::map<std::string,double> timestamps_;
};
}
