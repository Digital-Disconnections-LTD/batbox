#include "termio/Querier.hpp"
#include "termio/Csi.hpp"
namespace batbox::termio {
TerminalQuerier::TerminalQuerier(event_loop::Loop& loop,
  std::function<void(const std::string&)> stdoutWrite):loop_(loop),stdoutWrite_(std::move(stdoutWrite)){}
event_loop::Deferred<ResponseRef> TerminalQuerier::send(TerminalQuery query){
  event_loop::Deferred<ResponseRef> result(loop_);
  try{
    auto pending=std::make_shared<Pending>();pending->match=std::move(query.match);
    pending->resolve=[result](ResponseRef response){result.resolve(std::move(response));};
    queue_.push_back(std::move(pending));
    stdoutWrite_(query.request);
  }catch(...){result.reject(std::current_exception());}
  return result;
}
event_loop::Deferred<event_loop::Unit> TerminalQuerier::flush(){
  event_loop::Deferred<event_loop::Unit> result(loop_);
  try{
    auto pending=std::make_shared<Pending>();pending->sentinel=true;
    pending->resolveSentinel=[result]{result.resolve({});};
    queue_.push_back(std::move(pending));
    stdoutWrite_(csi(Json::array({"c"})));
  }catch(...){result.reject(std::current_exception());}
  return result;
}
void TerminalQuerier::onResponse(ResponseRef response){
  const auto length=queue_.size();
  for(std::size_t i=0;i<length;++i){
    if(i>=queue_.size())throw std::runtime_error("Cannot read properties of undefined (reading 'kind')");
    const auto pending=queue_[i];
    if(!pending->sentinel&&pending->match(response)){
      if(i<queue_.size()){
        const auto selected=queue_[i];queue_.erase(queue_.begin()+static_cast<std::ptrdiff_t>(i));
        if(!selected->sentinel)selected->resolve(std::move(response));
      }
      return;
    }
  }
  const auto& type=value::property(*response,"type");
  if(!type.is_string()||type.get<std::string>()!="da1")return;
  for(std::size_t i=0;i<queue_.size();++i){
    if(!queue_[i]->sentinel)continue;
    std::vector<std::shared_ptr<Pending>> drained(queue_.begin(),queue_.begin()+static_cast<std::ptrdiff_t>(i+1));
    queue_.erase(queue_.begin(),queue_.begin()+static_cast<std::ptrdiff_t>(i+1));
    for(const auto& pending:drained){if(pending->sentinel)pending->resolveSentinel();else pending->resolve({});}
    break;
  }
}
}
