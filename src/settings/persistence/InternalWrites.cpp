#include "settings/persistence/InternalWrites.hpp"
#include <stdexcept>

namespace batbox::settings::persistence {
InternalWrites::InternalWrites(Clock now):now_(std::move(now)){
  if(!now_)throw std::invalid_argument("Missing settings write clock");
}
void InternalWrites::markInternalWrite(const std::string& path){
  const std::lock_guard lock(mutex_);
  timestamps_[path]=now_();
}
bool InternalWrites::consumeInternalWrite(const std::string& path,
  double windowMs){
  const std::lock_guard lock(mutex_);
  const auto found=timestamps_.find(path);
  if(found!=timestamps_.end()&&now_()-found->second<windowMs){
    timestamps_.erase(found);return true;
  }
  return false;
}
void InternalWrites::clearInternalWrites(){
  const std::lock_guard lock(mutex_);timestamps_.clear();
}
}
