#include "interactive/navigation/Cancel.hpp"
#include <stdexcept>

namespace batbox::interactive::navigation {
namespace {
template<class T>void need(const T& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("Queue-cancel binding required: ")+name);
}
std::string imageKey(const Json& id){
  if(id.is_string())return id.get<std::string>();
  if(id.is_number())return value::numberToString(id.get<double>());
  if(id.is_boolean())return id.get<bool>()?"true":"false";
  if(id.is_null())return "null";
  return value::stringify(id);
}
}
void handleQueuedCommandOnCancel(const std::string& inputValue,
    const QueueCancelServices& s){
  need(s.nowMs,"nowMs");
  const auto result=s.queue.popAllEditable(inputValue,0,s.nowMs());
  if(!result)return;
  need(s.setInputValue,"setInputValue");s.setInputValue(result->text);
  need(s.setInputMode,"setInputMode");s.setInputMode("prompt");
  if(result->images.empty())return;
  need(s.updatePastedContents,"updatePastedContents");
  const auto images=result->images;
  s.updatePastedContents([images](const Json& previous){
    Json next=previous;
    if(!next.is_object())next=Json::object();
    for(const auto& image:images){
      const auto& id=value::property(image,"id");
      next[imageKey(id)]=image;
    }
    return next;
  });
}
} // namespace batbox::interactive::navigation
