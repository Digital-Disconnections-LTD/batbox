#pragma once
#include <array>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace batbox::bats {
class CompletionWaiter {
 public:
  void publish(int slot,std::uint64_t ticket){
    {
      std::lock_guard lock(mutex_);
      tickets_[static_cast<std::size_t>(slot)]=ticket;
    }
    cv_.notify_one();
  }
  int waitFirst(){
    std::unique_lock lock(mutex_);
    cv_.wait(lock,[&]{return tickets_[0]!=0 || tickets_[1]!=0;});
    if(tickets_[0]!=0 && (tickets_[1]==0 || tickets_[0]<tickets_[1]))return 0;
    return 1;
  }
 private:
  std::mutex mutex_;
  std::condition_variable cv_;
  std::array<std::uint64_t,2> tickets_{};
};

// Completion-mapping order adapter. The global ticket lock serializes mapping
// registration and producer settlement. Background mapping is registered early,
// before the iterator loop; message mapping is registered at each race. Tickets
// preserve enqueue order without sampling future readiness.
class CompletionSignal {
 public:
  void preRegisterMapping(){
    std::lock_guard order(orderMutex_);
    std::lock_guard lock(mutex_);
    preRegistered_=true;
    if(settled_ && mappingTicket_==0)mappingTicket_=nextTicket_++;
  }
  void subscribe(const std::shared_ptr<CompletionWaiter>& waiter,int slot){
    std::lock_guard order(orderMutex_);
    std::lock_guard lock(mutex_);
    ++subscriptions_;
    if(settled_){
      if(mappingTicket_==0)mappingTicket_=nextTicket_++;
      waiter->publish(slot,mappingTicket_);
    }
    else subscribers_.push_back({waiter,slot});
  }
  void settle(){
    std::lock_guard order(orderMutex_);
    std::lock_guard lock(mutex_);
    if(settled_)return;
    settled_=true;
    if(preRegistered_ || !subscribers_.empty())mappingTicket_=nextTicket_++;
    for(const auto& subscriber:subscribers_)
      if(auto waiter=subscriber.waiter.lock())
        waiter->publish(subscriber.slot,mappingTicket_);
    subscribers_.clear();
  }
  std::size_t subscriberCount()const{
    std::lock_guard lock(mutex_);
    return subscriptions_;
  }
 private:
  struct Subscriber {std::weak_ptr<CompletionWaiter> waiter;int slot;};
  mutable std::mutex mutex_;
  bool settled_=false,preRegistered_=false;
  std::uint64_t mappingTicket_=0;
  std::size_t subscriptions_=0;
  std::vector<Subscriber> subscribers_;
  inline static std::mutex orderMutex_;
  inline static std::uint64_t nextTicket_=1;
};
} // namespace batbox::bats
