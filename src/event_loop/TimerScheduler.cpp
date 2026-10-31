#include "event_loop/TimerScheduler.hpp"
namespace batbox::event_loop {
namespace {
class AdapterHandle final:public timers::TimerHandle {
public:
  explicit AdapterHandle(Loop::TimerHandle handle):handle_(std::move(handle)){}
  void cancel()override{handle_.cancel();}
private:
  Loop::TimerHandle handle_;
};
}
std::shared_ptr<timers::TimerHandle> TimerScheduler::schedule(double ms,Callback callback,bool unref){
  return std::make_shared<AdapterHandle>(loop_.setTimeout(ms,std::move(callback),!unref));
}
} // namespace batbox::event_loop
