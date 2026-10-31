#include "event_loop/Loop.hpp"
#include "event_loop/AsyncContext.hpp"
#include <uv.h>
#include <atomic>
#include <cmath>
#include <deque>
#include <exception>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>

namespace batbox::event_loop {
namespace {
void check(int code,const char* action){
  if(code<0)throw std::runtime_error(std::string(action)+": "+uv_strerror(code));
}
}
struct Loop::Core {
  struct Timer {
    uv_timer_t uv{};
    Core* owner=nullptr;
    std::uint64_t id=0;
    Callback callback;
    bool closed=false;
  };
  struct Hold {uv_async_t uv{};Core* owner=nullptr;std::uint64_t id=0;bool closed=false;};
  uv_loop_t uv{};
  uv_async_t wake{};
  uv_check_t nextTurnCheck{};
  std::thread::id owner=std::this_thread::get_id();
  std::atomic_bool closing=false;
  std::mutex missionsMutex;
  std::deque<Callback> missions;
  std::deque<Callback> microtasks;
  std::deque<Callback> nextTurnMissions;
  std::map<std::uint64_t,std::shared_ptr<Timer>> timers;
  std::map<std::uint64_t,std::shared_ptr<Hold>> holds;
  std::uint64_t nextTimerId=1;
  std::uint64_t nextHoldId=1;
  std::exception_ptr failure;
  bool running=false,stopRequested=false;
  Core(){
    check(uv_loop_init(&uv),"uv_loop_init");
    check(uv_async_init(&uv,&wake,[](uv_async_t* handle){
      auto* self=static_cast<Core*>(handle->data);self->drainMissions();
    }),"uv_async_init");
    wake.data=this;
    uv_unref(reinterpret_cast<uv_handle_t*>(&wake));
    check(uv_check_init(&uv,&nextTurnCheck),"uv_check_init next turn");
    nextTurnCheck.data=this;
  }
  void assertOwner()const{
    if(std::this_thread::get_id()!=owner)throw std::logic_error("event loop owner thread required");
  }
  void postMission(Callback callback){
    if(!callback)throw std::invalid_argument("event loop callback required");
    std::lock_guard lock(missionsMutex);
    if(closing)throw std::logic_error("event loop is closed");
    missions.push_back(runtime::bindAsyncContext(std::move(callback)));
    check(uv_async_send(&wake),"uv_async_send");
  }
  void queueMicrotask(Callback callback){
    assertOwner();
    if(!callback)throw std::invalid_argument("event loop microtask required");
    if(closing)throw std::logic_error("event loop is closed");
    microtasks.push_back(runtime::bindAsyncContext(std::move(callback)));
  }
  void queueNextTurn(Callback callback){
    assertOwner();
    if(!callback)throw std::invalid_argument("event loop next turn callback required");
    if(closing)throw std::logic_error("event loop is closed");
    nextTurnMissions.push_back(runtime::bindAsyncContext(std::move(callback)));
    if(!uv_is_active(reinterpret_cast<uv_handle_t*>(&nextTurnCheck)))
      check(uv_check_start(&nextTurnCheck,[](uv_check_t* handle){
        auto* owner=static_cast<Core*>(handle->data);
        const auto count=owner->nextTurnMissions.size();
        for(std::size_t index=0;index<count&&!owner->failure;++index){
          auto next=std::move(owner->nextTurnMissions.front());
          owner->nextTurnMissions.pop_front();
          owner->invoke(std::move(next));
        }
        if(owner->nextTurnMissions.empty())uv_check_stop(&owner->nextTurnCheck);
      }),"uv_check_start next turn");
  }
  void invoke(Callback callback){
    if(failure)return;
    try{callback();drainMicrotasks();}
    catch(...){failure=std::current_exception();uv_stop(&uv);}
  }
  void drainMicrotasks(){
    while(!microtasks.empty()&&!failure){
      auto callback=std::move(microtasks.front());microtasks.pop_front();
      try{callback();}catch(...){failure=std::current_exception();uv_stop(&uv);}
    }
  }
  void drainMissions(){
    for(;;){
      Callback callback;
      {std::lock_guard lock(missionsMutex);
        if(missions.empty()||failure)return;
        callback=std::move(missions.front());missions.pop_front();}
      invoke(std::move(callback));
    }
  }
  bool hasMissions(){std::lock_guard lock(missionsMutex);return !missions.empty();}
  void closeTimer(const std::shared_ptr<Timer>& timer){
    assertOwner();
    if(timer->closed)return;
    timer->closed=true;uv_timer_stop(&timer->uv);
    uv_close(reinterpret_cast<uv_handle_t*>(&timer->uv),[](uv_handle_t* handle){
      auto* timer=static_cast<Timer*>(handle->data);
      auto* owner=timer->owner;
      auto it=owner->timers.find(timer->id);
      if(it!=owner->timers.end()){auto keep=std::move(it->second);owner->timers.erase(it);}
    });
  }
  void applyTimer(std::uint64_t id,int operation){
    assertOwner();
    const auto it=timers.find(id);if(it==timers.end()||it->second->closed)return;
    auto* handle=reinterpret_cast<uv_handle_t*>(&it->second->uv);
    if(operation==0)closeTimer(it->second);
    else if(operation==1)uv_ref(handle);
    else uv_unref(handle);
  }
  void closeHold(std::uint64_t id){
    assertOwner();const auto it=holds.find(id);
    if(it==holds.end()||it->second->closed)return;
    auto hold=it->second;hold->closed=true;
    uv_close(reinterpret_cast<uv_handle_t*>(&hold->uv),[](uv_handle_t* handle){
      auto* hold=static_cast<Hold*>(handle->data);auto* owner=hold->owner;
      const auto it=owner->holds.find(hold->id);
      if(it!=owner->holds.end()){auto keep=std::move(it->second);owner->holds.erase(it);}
    });
  }
};
Loop::Loop():core_(std::make_shared<Core>()){}
Loop::~Loop(){
  if(!core_)return;
  auto core=std::move(core_);
  if(std::this_thread::get_id()!=core->owner)std::terminate();
  {std::lock_guard lock(core->missionsMutex);core->closing=true;}
  for(auto it=core->timers.begin();it!=core->timers.end();){
    auto timer=it->second;++it;core->closeTimer(timer);
  }
  for(auto it=core->holds.begin();it!=core->holds.end();){
    const auto id=it->first;++it;core->closeHold(id);
  }
  if(!uv_is_closing(reinterpret_cast<uv_handle_t*>(&core->wake)))
    uv_close(reinterpret_cast<uv_handle_t*>(&core->wake),nullptr);
  if(!uv_is_closing(reinterpret_cast<uv_handle_t*>(&core->nextTurnCheck)))
    uv_close(reinterpret_cast<uv_handle_t*>(&core->nextTurnCheck),nullptr);
  while(uv_loop_close(&core->uv)==UV_EBUSY)uv_run(&core->uv,UV_RUN_NOWAIT);
}
bool Loop::isOwnerThread()const{return std::this_thread::get_id()==core_->owner;}
bool Loop::Dispatcher::valid()const{
  const auto core=core_.lock();return core&&!core->closing;
}
bool Loop::Dispatcher::isOwnerThread()const{
  const auto core=core_.lock();return core&&std::this_thread::get_id()==core->owner;
}
bool Loop::Dispatcher::sameLoop(const Dispatcher& other)const{
  const auto lhs=core_.lock(),rhs=other.core_.lock();return lhs&&lhs==rhs;
}
void Loop::Dispatcher::postMission(Callback callback)const{
  const auto core=core_.lock();if(!core)throw std::logic_error("event loop expired");
  core->postMission(std::move(callback));
}
void Loop::Dispatcher::queueMicrotask(Callback callback)const{
  const auto core=core_.lock();if(!core)throw std::logic_error("event loop expired");
  core->queueMicrotask(std::move(callback));
}
void Loop::Dispatcher::queueNextTurn(Callback callback)const{
  const auto core=core_.lock();if(!core)throw std::logic_error("event loop expired");
  core->queueNextTurn(std::move(callback));
}
bool Loop::TimerHandle::valid()const{
  const auto core=dispatcher_.core_.lock();
  if(!core||!dispatcher_.isOwnerThread())return false;
  const auto it=core->timers.find(id_);return it!=core->timers.end()&&!it->second->closed;
}
void Loop::TimerHandle::cancel()const{
  const auto core=dispatcher_.core_.lock();if(!core||core->closing)return;
  if(dispatcher_.isOwnerThread())core->applyTimer(id_,0);
  else try{dispatcher_.postMission([weak=dispatcher_.core_,id=id_]{
    if(auto owner=weak.lock())owner->applyTimer(id,0);});}catch(const std::logic_error&){}
}
void Loop::TimerHandle::ref()const{
  const auto core=dispatcher_.core_.lock();if(!core||core->closing)return;
  if(dispatcher_.isOwnerThread())core->applyTimer(id_,1);
  else try{dispatcher_.postMission([weak=dispatcher_.core_,id=id_]{
    if(auto owner=weak.lock())owner->applyTimer(id,1);});}catch(const std::logic_error&){}
}
void Loop::TimerHandle::unref()const{
  const auto core=dispatcher_.core_.lock();if(!core||core->closing)return;
  if(dispatcher_.isOwnerThread())core->applyTimer(id_,2);
  else try{dispatcher_.postMission([weak=dispatcher_.core_,id=id_]{
    if(auto owner=weak.lock())owner->applyTimer(id,2);});}catch(const std::logic_error&){}
}
Loop::KeepAlive::KeepAlive(KeepAlive&& other)noexcept:
  dispatcher_(std::move(other.dispatcher_)),id_(std::exchange(other.id_,0)){}
Loop::KeepAlive& Loop::KeepAlive::operator=(KeepAlive&& other)noexcept{
  if(this!=&other){release();dispatcher_=std::move(other.dispatcher_);id_=std::exchange(other.id_,0);}
  return *this;
}
Loop::KeepAlive::~KeepAlive(){release();}
bool Loop::KeepAlive::active()const{
  if(id_==0||!dispatcher_.isOwnerThread())return false;
  const auto core=dispatcher_.core_.lock();if(!core)return false;
  const auto it=core->holds.find(id_);return it!=core->holds.end()&&!it->second->closed;
}
void Loop::KeepAlive::release(){
  const auto id=std::exchange(id_,0);if(id==0)return;
  const auto core=dispatcher_.core_.lock();if(!core||core->closing)return;
  if(dispatcher_.isOwnerThread())core->closeHold(id);
  else try{dispatcher_.postMission([weak=dispatcher_.core_,id]{
    if(auto owner=weak.lock())owner->closeHold(id);});}catch(...){}
}
std::uint64_t Loop::normalizeTimeout(double delayMs){
  if(!std::isfinite(delayMs)||delayMs<1||delayMs>2147483647.0)return 1;
  return static_cast<std::uint64_t>(std::trunc(delayMs));
}
Loop::TimerHandle Loop::setTimeout(double delayMs,Callback callback,bool referenced){
  auto& core=*core_;core.assertOwner();
  if(core.closing)throw std::logic_error("event loop is closed");
  if(!callback)throw std::invalid_argument("timer callback required");
  auto state=std::make_shared<Core::Timer>();state->owner=&core;
  state->id=core.nextTimerId++;state->callback=runtime::bindAsyncContext(std::move(callback));
  check(uv_timer_init(&core.uv,&state->uv),"uv_timer_init");
  state->uv.data=state.get();
  core.timers.emplace(state->id,state);
  check(uv_timer_start(&state->uv,[](uv_timer_t* handle){
    auto* state=static_cast<Core::Timer*>(handle->data);
    auto* owner=state->owner;
    auto callback=std::move(state->callback);
    owner->closeTimer(owner->timers.at(state->id));
    owner->invoke(std::move(callback));
  },normalizeTimeout(delayMs),0),"uv_timer_start");
  if(!referenced)uv_unref(reinterpret_cast<uv_handle_t*>(&state->uv));
  return TimerHandle(dispatcher(),state->id);
}
Loop::KeepAlive Loop::hold(){
  auto& core=*core_;core.assertOwner();
  if(core.closing)throw std::logic_error("event loop is closed");
  auto state=std::make_shared<Core::Hold>();state->owner=&core;state->id=core.nextHoldId++;
  check(uv_async_init(&core.uv,&state->uv,[](uv_async_t*){}),"uv_async_init keepalive");
  state->uv.data=state.get();core.holds.emplace(state->id,state);
  return KeepAlive(dispatcher(),state->id);
}
void Loop::queueWork(Callback operation,
    std::function<void(std::exception_ptr)> completion){
  auto core=core_;core->assertOwner();
  if(core->closing)throw std::logic_error("event loop is closed");
  if(!operation||!completion)throw std::invalid_argument("work callbacks required");
  struct Work {
    uv_work_t request{};
    std::shared_ptr<Core> owner;
    Callback operation;
    std::function<void(std::exception_ptr)> completion;
    std::exception_ptr failure;
  };
  auto work=std::make_unique<Work>();work->owner=core;
  work->operation=runtime::bindAsyncContext(std::move(operation));
  work->completion=runtime::bindAsyncContext(std::move(completion));
  work->request.data=work.get();
  check(uv_queue_work(&core->uv,&work->request,[](uv_work_t* request){
    auto* state=static_cast<Work*>(request->data);
    try{state->operation();}catch(...){state->failure=std::current_exception();}
  },[](uv_work_t* request,int status){
    std::unique_ptr<Work> state(static_cast<Work*>(request->data));
    if(state->owner->closing)return;
    if(status<0&&!state->failure)state->failure=std::make_exception_ptr(
      std::runtime_error(std::string("uv_queue_work: ")+uv_strerror(status)));
    state->owner->invoke([&]{state->completion(state->failure);});
  }),"uv_queue_work");
  work.release();
}
void Loop::run(){
  auto& core=*core_;core.assertOwner();
  if(core.running)throw std::logic_error("event loop run is not reentrant");
  core.running=true;core.stopRequested=false;
  try{
    for(;;){
      core.drainMicrotasks();core.drainMissions();
      if(core.failure)std::rethrow_exception(core.failure);
      if(core.stopRequested)break;
      if(!uv_loop_alive(&core.uv)){
        if(!core.hasMissions()&&core.microtasks.empty())break;
        continue;
      }
      uv_run(&core.uv,UV_RUN_DEFAULT);
      if(core.failure)std::rethrow_exception(core.failure);
      if(core.stopRequested)break;
    }
  }catch(...){core.running=false;throw;}
  core.running=false;
}
void Loop::runReady(){
  auto& core=*core_;core.assertOwner();
  if(core.running)throw std::logic_error("event loop run is not reentrant");
  core.running=true;
  try{
    core.drainMicrotasks();core.drainMissions();
    uv_ref(reinterpret_cast<uv_handle_t*>(&core.wake));
    uv_run(&core.uv,UV_RUN_NOWAIT);
    uv_unref(reinterpret_cast<uv_handle_t*>(&core.wake));
    core.drainMissions();core.drainMicrotasks();
    if(core.failure)std::rethrow_exception(core.failure);
  }catch(...){core.running=false;throw;}
  core.running=false;
}
void Loop::stop(){
  auto core=core_;
  if(std::this_thread::get_id()==core->owner){core->stopRequested=true;uv_stop(&core->uv);}
  else try{core->postMission([core]{core->stopRequested=true;uv_stop(&core->uv);});}
    catch(const std::logic_error&){}
}
} // namespace batbox::event_loop
