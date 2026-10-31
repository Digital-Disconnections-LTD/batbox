#include "cli/structured/InputReader.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::cli::structured {
InputReader::InputReader(event_loop::Loop& loop,
    std::shared_ptr<ControlResponses> control,
    std::shared_ptr<RequestProducer> producer,
    InputReaderServices services):loop_(loop),control_(std::move(control)),
    producer_(std::move(producer)),services_(std::move(services)){
  if(!control_||!producer_||!services_.nextChunk||
     !services_.logParsedMessageType)
    throw std::invalid_argument("Structured input imports required");
}
void InputReader::prependUserMessage(const std::string& content){
  prependedLines_.push_back(value::stringify(Json{{"type","user"},
    {"session_id",""},{"message",Json{{"role","user"},
      {"content",content}}},{"parent_tool_use_id",nullptr}})+"\n");
}
event_loop::Deferred<StreamItem<Json>> InputReader::next(){
  event_loop::Deferred<StreamItem<Json>> output(loop_);
  if(closed_){output.resolve({true,{}});return output;}
  waiting_.push_back(output);
  pump();
  return output;
}
void InputReader::deliver(Json message,bool fromSplit){
  try{
    if(fromSplit){
      const auto& type=value::property(message,"type");
      services_.logParsedMessageType(type.is_string()?type.get<std::string>():
        std::string());
    }
    auto output=std::move(waiting_.front());waiting_.pop_front();
    output.resolve({false,std::move(message)});
  }catch(...){fail(std::current_exception());}
}
void InputReader::consumeLine(std::string line,bool fromSplit){
  processing_=true;
  try{
    auto pending=processLine(loop_,line,*control_,services_.processLine);
    const auto self=shared_from_this();
    pending.then([self,fromSplit](const std::optional<Json>* message,
                                  const std::exception_ptr& error){
      self->processing_=false;
      if(error){self->fail(error);return;}
      if(message&&*message)self->deliver(**message,fromSplit);
      self->pump();
    });
  }catch(...){processing_=false;fail(std::current_exception());}
}
void InputReader::finish(){
  try{
    producer_->closeInput();
    closed_=true;
    auto output=std::move(waiting_.front());waiting_.pop_front();
    output.resolve({true,{}});
    while(!waiting_.empty()){
      auto next=std::move(waiting_.front());waiting_.pop_front();
      next.resolve({true,{}});
    }
  }catch(...){fail(std::current_exception());}
}
void InputReader::fail(std::exception_ptr error){
  closed_=true;
  while(!waiting_.empty()){
    auto output=std::move(waiting_.front());waiting_.pop_front();
    output.reject(error);
  }
}
void InputReader::pump(){
  if(processing_||closed_||waiting_.empty())return;
  if(splitPhase_){
    if(!prependedLines_.empty()){
      std::string prefix;
      for(const auto& item:prependedLines_)prefix+=item;
      content_=prefix+content_;
      prependedLines_.clear();
    }
    if(const auto newline=content_.find('\n');newline!=std::string::npos){
      auto line=content_.substr(0,newline);
      content_.erase(0,newline+1);
      consumeLine(std::move(line),true);return;
    }
    splitPhase_=false;
  }
  if(!eof_){
    processing_=true;
    try{
      auto chunk=services_.nextChunk();
      const auto self=shared_from_this();
      chunk.then([self](const std::optional<std::string>* value,
                        const std::exception_ptr& error){
        self->processing_=false;
        if(error){self->fail(error);return;}
        if(value&&*value){
          self->content_+=**value;
          self->splitPhase_=true;
        }else self->eof_=true;
        self->pump();
      });
    }catch(...){processing_=false;fail(std::current_exception());}
    return;
  }
  if(!tailProcessed_&&!content_.empty()){
    tailProcessed_=true;
    auto line=std::move(content_);content_.clear();
    consumeLine(std::move(line),false);return;
  }
  finish();
}
}
