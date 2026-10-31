#pragma once
#include "bootstrap/Signal.hpp"
#include <cstdint>
#include <optional>

namespace batbox::interactive {
class QueryGuard {
public:
  bool reserve();
  void cancelReservation();
  std::optional<std::uint64_t> tryStart();
  bool end(std::uint64_t generation);
  void forceEnd();
  bool isActive()const{return status_!=Status::Idle;}
  bool isRunning()const{return status_==Status::Running;}
  std::uint64_t generation()const{return generation_;}
  bool getSnapshot()const{return isActive();}
  std::function<void()> subscribe(std::shared_ptr<bootstrap::Signal<>::Listener> listener){
    return changed_.subscribe(std::move(listener));
  }
private:
  enum class Status {Idle,Dispatching,Running};
  Status status_=Status::Idle;
  std::uint64_t generation_=0;
  bootstrap::Signal<> changed_;
};
} // namespace batbox::interactive
