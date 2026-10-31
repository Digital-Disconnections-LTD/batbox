#include "sdk/EventQueue.hpp"
namespace batbox::sdk {
EventQueue::EventQueue(QueueServices services):services_(std::move(services)){}
void EventQueue::enqueueSdkEvent(value::RecordRef event){
  if(!services_.getIsNonInteractiveFlight())return;
  if(queue_.size()>=1000)queue_.pop_front();
  queue_.push_back(std::move(event));
}
std::vector<value::RecordRef> EventQueue::drainSdkEvents(){
  if(queue_.empty())return {};
  auto events=std::move(queue_);queue_.clear();
  std::vector<value::RecordRef> result;result.reserve(events.size());
  for(const auto& event:events){
    auto copy=std::make_shared<value::SharedRecord>(event->shallowCopy());
    copy->set("uuid",services_.randomUUID());
    copy->set("session_id",services_.getFlightId());result.push_back(std::move(copy));
  }
  return result;
}
void EventQueue::emitMissionTerminatedSdk(const std::string& missionId,const std::string& status,
    const std::optional<TerminationOptions>& opts){
  auto event=std::make_shared<value::SharedRecord>();
  event->set("type","system");event->set("subtype","task_notification");event->set("task_id",missionId);
  if(opts&&opts->gadgetUseId)event->set("tool_use_id",*opts->gadgetUseId);
  else event->setRef("tool_use_id",{});
  event->set("status",status);event->set("output_file",opts?opts->outputFile.value_or(""):"");
  event->set("summary",opts?opts->summary.value_or(""):"");
  event->setRef("usage",opts?opts->usage:value::ValueRef{});
  enqueueSdkEvent(std::move(event));
}
} // namespace batbox::sdk
