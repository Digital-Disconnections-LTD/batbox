#include "interactive/QueryGuard.hpp"

namespace batbox::interactive {
bool QueryGuard::reserve(){
  if(status_!=Status::Idle)return false;
  status_=Status::Dispatching;changed_.emit();return true;
}
void QueryGuard::cancelReservation(){
  if(status_!=Status::Dispatching)return;
  status_=Status::Idle;changed_.emit();
}
std::optional<std::uint64_t> QueryGuard::tryStart(){
  if(status_==Status::Running)return {};
  status_=Status::Running;++generation_;changed_.emit();return generation_;
}
bool QueryGuard::end(std::uint64_t generation){
  if(generation_!=generation||status_!=Status::Running)return false;
  status_=Status::Idle;changed_.emit();return true;
}
void QueryGuard::forceEnd(){
  if(status_==Status::Idle)return;
  status_=Status::Idle;++generation_;changed_.emit();
}
} // namespace batbox::interactive
