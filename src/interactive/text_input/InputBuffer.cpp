#include "interactive/text_input/InputBuffer.hpp"
#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace batbox::interactive::text_input {
struct InputBuffer::State {
  std::vector<BufferEntryRef> buffer;
  std::int64_t currentIndex=-1,maxSize;
  double lastPushTime=0,debounceMs;
  InputBufferServices services;
  InputBufferServices::Cancel cancel;
};
namespace {
void push(const std::shared_ptr<InputBuffer::State>& s,std::string text,
    double cursorOffset,std::shared_ptr<Json> pastedContents,
    std::int64_t capturedIndex,std::size_t capturedLength){
  const auto now=s->services.now();
  if(s->cancel){auto cancel=std::move(s->cancel);s->cancel={};cancel();}
  if(now-s->lastPushTime<s->debounceMs){
    std::weak_ptr<InputBuffer::State> weak=s;
    s->cancel=s->services.setTimeout(s->debounceMs,
      [weak,text=std::move(text),cursorOffset,pastedContents=std::move(pastedContents),
          capturedIndex,capturedLength]{
        if(auto state=weak.lock())push(state,text,cursorOffset,pastedContents,
          capturedIndex,capturedLength);
      });return;
  }
  s->lastPushTime=now;
  auto next=s->buffer;
  if(capturedIndex>=0){
    const auto count=std::min(next.size(),static_cast<std::size_t>(capturedIndex+1));
    next.resize(count);
  }
  if(next.empty()||next.back()->text!=text){
    next.push_back(std::make_shared<BufferEntry>(BufferEntry{
      std::move(text),cursorOffset,std::move(pastedContents),now}));
    if(static_cast<std::int64_t>(next.size())>s->maxSize){
      const auto length=static_cast<std::int64_t>(next.size());
      const auto start=s->maxSize>0?std::max<std::int64_t>(length-s->maxSize,0):
        std::min(-s->maxSize,length);
      next.erase(next.begin(),next.begin()+start);
    }
  }
  s->buffer=std::move(next);
  const auto nextIndex=s->currentIndex>=0?s->currentIndex+1:
    static_cast<std::int64_t>(capturedLength);
  s->currentIndex=std::min(nextIndex,s->maxSize-1);
}
}
InputBuffer::InputBuffer(std::int64_t maxSize,double debounceMs,InputBufferServices services)
  :state_(std::make_shared<State>()){
  if(!services.now||!services.setTimeout)
    throw std::invalid_argument("Input buffer clock and timeout bindings required");
  state_->maxSize=maxSize;state_->debounceMs=debounceMs;state_->services=std::move(services);
}
InputBuffer::~InputBuffer(){if(state_->cancel)state_->cancel();}
void InputBuffer::push(std::string text,double cursorOffset,std::shared_ptr<Json> pastedContents){
  if(!pastedContents)pastedContents=std::make_shared<Json>(Json::object());
  text_input::push(state_,std::move(text),cursorOffset,std::move(pastedContents),
    state_->currentIndex,state_->buffer.size());
}
BufferEntryRef InputBuffer::undo(){
  if(state_->currentIndex<0||state_->buffer.empty())return {};
  const auto target=std::max<std::int64_t>(0,state_->currentIndex-1);
  if(static_cast<std::size_t>(target)>=state_->buffer.size())return {};
  state_->currentIndex=target;return state_->buffer[static_cast<std::size_t>(target)];
}
void InputBuffer::clear(){
  state_->buffer.clear();state_->currentIndex=-1;state_->lastPushTime=0;
  if(state_->cancel){auto cancel=std::move(state_->cancel);state_->cancel={};cancel();}
}
bool InputBuffer::canUndo()const{return state_->currentIndex>0&&state_->buffer.size()>1;}
std::int64_t InputBuffer::index()const{return state_->currentIndex;}
const std::vector<BufferEntryRef>& InputBuffer::entries()const{return state_->buffer;}
InputBufferServices indigenousInputBufferServices(event_loop::Loop& owner){
  return {[]{return std::chrono::duration<double,std::milli>(
      std::chrono::system_clock::now().time_since_epoch()).count();},
    [&owner](double delay,std::function<void()> callback){
      if(!owner.isOwnerThread())throw std::logic_error("Input buffer requires owner loop");
      auto timer=owner.setTimeout(delay,std::move(callback));
      return [timer]{timer.cancel();};
    }};
}
} // namespace batbox::interactive::text_input
