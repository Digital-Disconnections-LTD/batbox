#pragma once
#include "transport/SSE.hpp"
#include <functional>

namespace batbox::mcp {
struct RemoteEvent {std::string event,data;std::optional<std::string> id;};
class EventStream {
public:
  void feed(std::string_view bytes,const std::function<void(const RemoteEvent&)>& receive){
    consume(lines_.decode(bytes),receive);
  }
  void flush(const std::function<void(const RemoteEvent&)>& receive){consume(lines_.flush(),receive);}
private:
  transport::LineDecoder lines_;
  RemoteEvent event_;
  bool hasData_=false;
  void consume(const std::vector<std::string>& lines,
      const std::function<void(const RemoteEvent&)>& receive){
    for(const auto& line:lines){
      if(line.empty()){
        if(hasData_)receive(event_);
        auto id=event_.id;event_=RemoteEvent{};event_.id=std::move(id);hasData_=false;
        continue;
      }
      if(line.front()==':')continue;
      const auto colon=line.find(':');const auto field=line.substr(0,colon);
      auto value=colon==std::string::npos?std::string{}:line.substr(colon+1);
      if(!value.empty()&&value.front()==' ')value.erase(0,1);
      if(field=="event")event_.event=std::move(value);
      else if(field=="id"&&value.find('\0')==std::string::npos)event_.id=std::move(value);
      else if(field=="data"){
        if(hasData_)event_.data+='\n';
        event_.data+=value;hasData_=true;
      }
    }
  }
};
} // namespace batbox::mcp
