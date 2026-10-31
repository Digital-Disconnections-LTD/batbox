#pragma once
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <thread>

namespace batbox::event_loop {
using Callback=std::function<void()>;

class Loop {
  struct Core;
public:
  class TimerHandle;
  class KeepAlive;
  class Dispatcher {
  public:
    Dispatcher()=default;
    bool valid()const;
    bool isOwnerThread()const;
    bool sameLoop(const Dispatcher& other)const;
    // Safe from any thread. The mission runs on the owner thread.
    void postMission(Callback callback)const;
    // Owner only. FIFO Promise/queueMicrotask job, never inline.
    void queueMicrotask(Callback callback)const;
    // Owner only. Runs in a later libuv check turn, like setImmediate.
    void queueNextTurn(Callback callback)const;
  private:
    friend class Loop;
    friend class TimerHandle;
    friend class KeepAlive;
    explicit Dispatcher(std::weak_ptr<Core> core):core_(std::move(core)){}
    std::weak_ptr<Core> core_;
  };
  class TimerHandle {
  public:
    TimerHandle()=default;
    bool valid()const;
    void cancel()const;
    void ref()const;
    void unref()const;
  private:
    friend class Loop;
    TimerHandle(Dispatcher dispatcher,std::uint64_t id):dispatcher_(std::move(dispatcher)),id_(id){}
    Dispatcher dispatcher_;
    std::uint64_t id_=0;
  };
  class KeepAlive {
  public:
    KeepAlive()=default;
    KeepAlive(const KeepAlive&)=delete;
    KeepAlive& operator=(const KeepAlive&)=delete;
    KeepAlive(KeepAlive&& other)noexcept;
    KeepAlive& operator=(KeepAlive&& other)noexcept;
    ~KeepAlive();
    void release();
    bool active()const;
  private:
    friend class Loop;
    KeepAlive(Dispatcher dispatcher,std::uint64_t id):dispatcher_(std::move(dispatcher)),id_(id){}
    Dispatcher dispatcher_;
    std::uint64_t id_=0;
  };

  Loop();
  ~Loop();
  Loop(const Loop&)=delete;
  Loop& operator=(const Loop&)=delete;
  Dispatcher dispatcher()const{return Dispatcher(core_);}
  bool isOwnerThread()const;
  void postMission(Callback callback)const{dispatcher().postMission(std::move(callback));}
  void queueMicrotask(Callback callback)const{dispatcher().queueMicrotask(std::move(callback));}
  void queueNextTurn(Callback callback)const{dispatcher().queueNextTurn(std::move(callback));}
  TimerHandle setTimeout(double delayMs,Callback callback,bool referenced=true);
  // Owner only. Blocking work uses libuv's shared worker pool; completion and
  // both callbacks restore the submitting async context on the owner.
  void queueWork(Callback operation,
    std::function<void(std::exception_ptr)> completion);
  KeepAlive hold();
  // Runs until no referenced handles/missions remain, or stop() is requested.
  void run();
  // Executes pending missions and timers already due without blocking.
  void runReady();
  void stop();
  static std::uint64_t normalizeTimeout(double delayMs);
private:
  std::shared_ptr<Core> core_;
};
} // namespace batbox::event_loop
