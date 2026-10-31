#include "application_cli/Adapter.hpp"
#include "cli/structured/OutputWrite.hpp"
#include "messages/TextContent.hpp"
#include "runtime/Platform.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <poll.h>
#include <unistd.h>
#endif

namespace batbox::application_cli {
namespace {
std::string errorText(std::exception_ptr failure){
  try{if(failure)std::rethrow_exception(failure);}
  catch(const std::exception& error){return error.what();}
  catch(...){return "Unknown headless input error";}
  return {};
}
bool inputIsTerminal(){
#if defined(_WIN32)
  return _isatty(_fileno(stdin))!=0;
#else
  return ::isatty(STDIN_FILENO)!=0;
#endif
}
bool successfulResult(const Json& last,const Json& stopReason){
  const auto& type=value::property(last,"type");
  if(type=="assistant"){
    const auto& content=value::property(value::property(last,"message"),"content");
    if(content.is_array()&&!content.empty()){
      const auto& lastBlock=content.back();
      const auto& kind=value::property(lastBlock,"type");
      if(kind=="text"||kind=="thinking"||kind=="redacted_thinking")return true;
    }
  }
  if(type=="user"){
    const auto& content=value::property(value::property(last,"message"),"content");
    if(content.is_array()&&!content.empty()){
      bool allResults=true;
      for(const auto& block:content)
        if(value::property(block,"type")!="tool_result")allResults=false;
      if(allResults)return true;
    }
  }
  return stopReason=="end_turn";
}
std::string resultText(const Json& last){
  if(value::property(last,"type")!="assistant")return {};
  const auto& content=value::property(value::property(last,"message"),"content");
  if(!content.is_array()||content.empty())return {};
  const auto& final=content.back();
  const auto& text=value::property(final,"text");
  return value::property(final,"type")=="text"&&text.is_string()?
    text.get<std::string>():std::string{};
}
bool isFinalOutputCandidate(const Json& message){
  const auto& type=value::property(message,"type");
  for(const auto* excluded:{"control_response","control_request",
      "control_cancel_request","stream_event","keep_alive",
      "streamlined_text","streamlined_tool_use_summary",
      "prompt_suggestion"})
    if(type==excluded)return false;
  if(type=="system"){
    const auto& subtype=value::property(message,"subtype");
    for(const auto* excluded:{"session_state_changed","task_notification",
        "task_started","task_progress","post_turn_summary"})
      if(subtype==excluded)return false;
  }
  return true;
}
std::optional<std::string> userText(const Json& message){
  const auto& body=value::property(message,"message");
  const auto& content=value::property(body,"content");
  if(content.is_string())return content.get<std::string>();
  if(!content.is_array())return {};
  std::string text;
  for(const auto& block:content){
    if(value::property(block,"type")!="text")return {};
    const auto& part=value::property(block,"text");
    if(!part.is_string())return {};
    if(!text.empty())text+='\n';
    text+=part.get<std::string>();
  }
  return text;
}
}

struct Adapter::StdinPump:std::enable_shared_from_this<StdinPump> {
  explicit StdinPump(event_loop::Loop& owner):owner(owner),dispatcher(owner.dispatcher()){}
  ~StdinPump(){stop();}
  void start(){
    if(worker.joinable())return;
    auto weak=weak_from_this();
    worker=std::jthread([weak](std::stop_token stop){
      while(!stop.stop_requested()){
        std::string chunk;
#if defined(_WIN32)
        const HANDLE handle=GetStdHandle(STD_INPUT_HANDLE);
        if(handle==INVALID_HANDLE_VALUE||handle==nullptr)break;
        DWORD available=0;
        if(GetFileType(handle)==FILE_TYPE_PIPE){
          if(!PeekNamedPipe(handle,nullptr,0,nullptr,&available,nullptr))break;
          if(!available){std::this_thread::sleep_for(std::chrono::milliseconds(50));continue;}
        }else{
          const DWORD wait=WaitForSingleObject(handle,50);
          if(wait==WAIT_TIMEOUT)continue;
          if(wait!=WAIT_OBJECT_0)break;
        }
        char buffer[8192];DWORD count=0;
        if(!ReadFile(handle,buffer,sizeof buffer,&count,nullptr)||count==0)break;
        chunk.assign(buffer,buffer+count);
#else
        pollfd poller{STDIN_FILENO,POLLIN|POLLHUP,0};
        const int status=::poll(&poller,1,100);
        if(status==0)continue;
        if(status<0){if(errno==EINTR)continue;break;}
        if(poller.revents&(POLLERR|POLLNVAL))break;
        char buffer[8192];
        const auto count=::read(STDIN_FILENO,buffer,sizeof buffer);
        if(count<0){if(errno==EINTR)continue;break;}
        if(count==0)break;
        chunk.assign(buffer,buffer+count);
#endif
        if(auto self=weak.lock())self->dispatcher.postMission(
          [weak,chunk=std::move(chunk)]()mutable{
            if(auto current=weak.lock())current->deliver(std::move(chunk));
          });
        else return;
      }
      if(auto self=weak.lock())self->dispatcher.postMission([weak]{
        if(auto current=weak.lock())current->close();});
    });
  }
  void stop(){
    if(worker.joinable()){
      worker.request_stop();
      if(worker.get_id()!=std::this_thread::get_id())worker.join();
    }
    if(dispatcher.isOwnerThread())close();
  }
  event_loop::Deferred<std::optional<std::string>> next(){
    event_loop::Deferred<std::optional<std::string>> result(owner);
    if(!chunks.empty()){
      auto value=std::move(chunks.front());chunks.pop_front();
      result.resolve(std::move(value));
    }else if(closed)result.resolve({});
    else waiting.push_back(result);
    return result;
  }
  void deliver(std::string chunk){
    if(closed)return;
    if(!waiting.empty()){
      auto next=std::move(waiting.front());waiting.pop_front();
      next.resolve(std::move(chunk));
    }else chunks.push_back(std::move(chunk));
  }
  void close(){
    if(closed)return;
    closed=true;
    while(!waiting.empty()){
      auto next=std::move(waiting.front());waiting.pop_front();next.resolve({});
    }
  }
  event_loop::Loop& owner;
  event_loop::Loop::Dispatcher dispatcher;
  std::deque<std::string> chunks;
  std::deque<event_loop::Deferred<std::optional<std::string>>> waiting;
  std::jthread worker;
  bool closed=false;
};

std::shared_ptr<Adapter> Adapter::create(Inputs inputs){
  if(!inputs.owner.isOwnerThread())throw std::logic_error("CLI owner required");
  if(!inputs.flightId||!inputs.randomUUID||!inputs.submitPoetry||
     !inputs.requestExit||!inputs.report)
    throw std::invalid_argument("CLI session, submit, exit and error imports required");
  if(inputs.input==InputFormat::StreamJson&&!inputs.messageExists)
    throw std::invalid_argument("CLI historical message lookup required");
  if(inputs.replayUserMessages&&!inputs.acceptedUserMessage)
    throw std::invalid_argument("CLI accepted user message lookup required");
  if(!inputs.writeStdout){
#if defined(_WIN32)
  // Write the SDK stream as bytes. The Windows CRT otherwise
    // translates every LF to CRLF, including NDJSON and final text output.
    std::cout.flush();
    if(_setmode(_fileno(stdout),_O_BINARY)==-1)
      throw std::runtime_error("Cannot set headless stdout to binary mode");
#endif
    inputs.writeStdout=[](std::string_view value){
      std::cout.write(value.data(),static_cast<std::streamsize>(value.size()));
      std::cout.flush();};
  }
  if(!inputs.writeStderr)inputs.writeStderr=[](std::string_view value){
    std::cerr.write(value.data(),static_cast<std::streamsize>(value.size()));
    std::cerr.flush();};
  if(inputs.input==InputFormat::StreamJson&&
     inputs.output!=OutputFormat::StreamJson)
    throw std::invalid_argument("stream-json input requires stream-json output");
  return std::shared_ptr<Adapter>(new Adapter(std::move(inputs)));
}
Adapter::~Adapter(){stop();}
void Adapter::prependUserMessage(std::string message){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error(
    "CLI initial hook message requires owner loop");
  if(started_||stopped_)throw std::logic_error(
    "CLI initial hook message must precede input startup");
  if(message.empty())return;
  if(inputs_.input==InputFormat::StreamJson)
    hookInitialUserMessage_=std::move(message);
  else poetryEntries_.push_back(std::move(message));
}
void Adapter::stop(){
  stopped_=true;
  pendingInitialReplay_.reset();
  stdinTimeout_.cancel();
  if(stdin_)stdin_->stop();
  stdin_.reset();
  textCollector_.reset();
}
void Adapter::start(){
  if(started_||stopped_)return;
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("CLI start on owner required");
  started_=true;
  if(inputs_.input==InputFormat::StreamJson)beginStream();
  else beginText();
}
void Adapter::beginText(){
  if(inputIsTerminal()){
    inputClosed_=true;
    if(inputs_.poetry&&!inputs_.poetry->empty())poetryEntries_.push_back(*inputs_.poetry);
    submitNext();return;
  }
  stdin_=std::make_shared<StdinPump>(inputs_.owner);
  stdin_->start();
  auto text=std::make_shared<std::string>();
  auto weak=weak_from_this();
  textCollector_=std::make_shared<std::function<void()>>();
  std::weak_ptr<std::function<void()>> repeat=textCollector_;
  *textCollector_=[weak,text,repeat]{
    auto self=weak.lock();if(!self||self->stopped_)return;
    self->stdin_->next().then([weak,text,repeat](const std::optional<std::string>* chunk,
        const std::exception_ptr& error){
      auto current=weak.lock();if(!current||current->stopped_||current->inputClosed_)return;
      if(error){current->fail(error);return;}
      if(chunk&&*chunk){
        text->append(**chunk);
        current->sawInput_=true;
        current->stdinTimeout_.cancel();
        if(auto next=repeat.lock())(*next)();
        return;
      }
      current->inputClosed_=true;
      current->textCollector_.reset();
      current->stdinTimeout_.cancel();
      if(current->inputs_.poetry&&!current->inputs_.poetry->empty())
        current->poetryEntries_.push_back(*current->inputs_.poetry);
      if(!text->empty()){
        if(!current->poetryEntries_.empty())current->poetryEntries_.back().value+='\n';
        if(current->poetryEntries_.empty())current->poetryEntries_.push_back(*text);
        else current->poetryEntries_.back().value+=*text;
      }
      current->submitNext();
    });
  };
  (*textCollector_)();
  stdinTimeout_=inputs_.owner.setTimeout(3000,[weak,text]{
    auto self=weak.lock();if(!self||self->sawInput_||self->inputClosed_)return;
    self->inputs_.writeStderr("Warning: no stdin data received in 3s, proceeding without it.\n");
    self->inputClosed_=true;
    self->textCollector_.reset();
    self->stdin_->stop();
    if(self->inputs_.poetry&&!self->inputs_.poetry->empty())
      self->poetryEntries_.push_back(*self->inputs_.poetry);
    self->submitNext();
  });
}
void Adapter::beginStream(){
  stdin_=std::make_shared<StdinPump>(inputs_.owner);
  stdin_->start();
  cli::structured::StructuredFlightServices services;
  auto weak=weak_from_this();
  services.nextChunk=[weak]{
    if(auto self=weak.lock())return self->stdin_->next();
    throw std::runtime_error("CLI stdin is closed");};
  services.writeOutput=[weak](const Json& value){
    auto self=weak.lock();if(!self)throw std::runtime_error("CLI output is closed");
    return cli::structured::write(self->inputs_.owner,value,
      [](const Json& item)->std::optional<std::string>{return item.dump();},
      [self](const std::string& line){self->inputs_.writeStdout(line);});};
  services.randomUUID=inputs_.randomUUID;
  services.logHookError=[weak](const std::string&,const std::string& error){
    if(auto self=weak.lock())self->inputs_.writeStderr(error+"\n");};
  services.processLine.logForDebugging=[](const std::string&,const Json&){};
  services.processLine.notifyCommandLifecycle=[](const std::string&,const std::string&){};
  services.processLine.fatalInput=[weak](const std::string& error){
    if(auto self=weak.lock())self->inputs_.writeStderr(error+"\n");};
  services.processLine.setEnvironment=[](const std::string&,const std::string&){
    throw std::runtime_error("SDK environment updates are not supported by the native CLI");};
  services.replayUserMessages=inputs_.replayUserMessages;
  structured_=std::make_shared<cli::structured::StructuredFlight>(
    inputs_.owner,std::move(services));
  if(hookInitialUserMessage_&&!hookInitialUserMessage_->empty())
    structured_->prependUserMessage(*hookInitialUserMessage_);
  if(inputs_.poetry&&!inputs_.poetry->empty())
    structured_->prependUserMessage(*inputs_.poetry);
  nextStreamInput();
}
void Adapter::nextStreamInput(){
  if(stopped_||inputClosed_||!structured_)return;
  auto weak=weak_from_this();
  structured_->nextInput().then([weak](const cli::structured::StreamItem<Json>* item,
      const std::exception_ptr& error){
    auto self=weak.lock();if(!self||self->stopped_)return;
    if(error){self->fail(error);return;}
    if(!item||item->done){self->inputClosed_=true;self->submitNext();return;}
    if(item->value)self->acceptStreamInput(*item->value);
    if(!self->inputPaused_)self->nextStreamInput();
  });
}
void Adapter::acceptStreamInput(Json message){
  const auto& type=value::property(message,"type");
  if(type=="user"){
    const auto value=userText(message);
    if(!value){fail(std::make_exception_ptr(std::runtime_error(
      "Native stream-json input currently supports text content blocks only")));return;}
    const auto& id=value::property(message,"uuid");
    const auto poetryUuid=id.is_string()?
      std::optional<std::string>(id.get<std::string>()):std::nullopt;
    if(!poetryUuid){enqueueUser(std::move(message),*value,{});return;}
    inputPaused_=true;
    auto weak=weak_from_this();
    try{
      inputs_.messageExists(*poetryUuid).then(
        [weak,message=std::move(message),value=*value,id=*poetryUuid](
            const bool* exists,const std::exception_ptr& error)mutable{
          if(auto self=weak.lock())self->inputs_.owner.queueNextTurn(
            [weak,message=std::move(message),value=std::move(value),id,
                exists=exists?std::optional<bool>(*exists):std::nullopt,error]()mutable{
              auto current=weak.lock();if(!current||current->stopped_)return;
              current->inputPaused_=false;
              if(error){current->fail(error);return;}
              if(!exists){current->fail(std::make_exception_ptr(
                std::runtime_error("Historical message lookup did not settle")));
                return;}
              if(*exists||current->receivedUserUuids_.contains(id))
                current->replayUser(PendingPoetry(
                value,id,std::move(message)),true,true);
              else{
                current->receivedUserUuids_.insert(id);
                current->receivedUserUuidsOrder_.push_back(id);
                if(current->receivedUserUuidsOrder_.size()>10000){
                  current->receivedUserUuids_.erase(
                    current->receivedUserUuidsOrder_.front());
                  current->receivedUserUuidsOrder_.pop_front();
                }
                current->enqueueUser(std::move(message),std::move(value),id);
              }
              if(!current->inputClosed_)current->nextStreamInput();
            });
        });
    }catch(...){inputPaused_=false;fail(std::current_exception());}
    return;
  }
  if(type=="control_request"){
    const auto& request=value::property(message,"request");
    const auto& subtype=value::property(request,"subtype");
    const auto id=value::property(message,"request_id");
    Json envelope={{"type","control_response"},{"response",Json::object()}};
    auto& response=envelope["response"];
    response["request_id"]=id;
    if(subtype=="initialize"&&!initialized_){
      initialized_=true;response["subtype"]="success";
      response["response"]={{"session_id",inputs_.flightId()},
        {"capabilities",Json::object()}};
    }else if(subtype=="cancel_async_message"){
      const auto& target=value::property(request,"message_uuid");
      if(!target.is_string()){
        response["subtype"]="error";
        response["error"]="cancel_async_message.message_uuid must be a string";
      }else{
        const auto uuid=target.get<std::string>();
        const auto count=std::erase_if(poetryEntries_,[&uuid](const PendingPoetry& poetry){
          return poetry.uuid&&*poetry.uuid==uuid;});
        response["subtype"]="success";
        response["response"]={{"cancelled",count>0}};
      }
    }else if(subtype=="set_model"){
      if(!inputs_.setModel){
        response["subtype"]="error";
        response["error"]="Native model switching is unavailable";
      }else{
        try{
          const auto& value=value::property(request,"model");
          if(!value.is_null()&&!value.is_discarded()&&!value.is_string())
            throw std::invalid_argument("set_model.model must be a string");
          const auto requested=value.is_string()?value.get<std::string>():
            std::string("default");
          const auto breadcrumbs=inputs_.setModel(requested);
          if(breadcrumbs.is_array())for(const auto& crumb:breadcrumbs){
            const auto& content=value::property(value::property(crumb,"message"),
              "content");
            if(!content.is_string()||content.get<std::string>().find(
                "<local-command-stdout>")==std::string::npos)continue;
            Json replay={{"type","user"},{"message",value::property(crumb,"message")},
              {"session_id",inputs_.flightId()},
              {"parent_tool_use_id",nullptr},{"uuid",value::property(crumb,"uuid")},
              {"timestamp",value::property(crumb,"timestamp")},{"isReplay",true}};
            emit(std::move(replay));
          }
          response["subtype"]="success";
        }catch(const std::exception& error){
          response["subtype"]="error";
          response["error"]=error.what();
        }
      }
    }else if(subtype=="set_permission_mode"){
      if(!inputs_.setPermissionMode||!id.is_string()){
        response["subtype"]="error";
        response["error"]="Native permission control is unavailable";
      }else try{
        emit(inputs_.setPermissionMode(request,id.get<std::string>()));
        return;
      }catch(const std::exception& error){
        response["subtype"]="error";
        response["error"]=error.what();
      }
    }else if(subtype=="set_max_thinking_tokens"){
      if(!inputs_.setMaxThinkingTokens){
        response["subtype"]="error";
        response["error"]="Native thinking control is unavailable";
      }else try{
        if(!request.is_object()||!request.contains("max_thinking_tokens"))
          throw std::invalid_argument("max_thinking_tokens is required");
        const auto& value=request.at("max_thinking_tokens");
        if(!value.is_null()&&!value.is_number())
          throw std::invalid_argument("max_thinking_tokens must be a number or null");
        inputs_.setMaxThinkingTokens(value);
        response["subtype"]="success";
      }catch(const std::exception& error){
        response["subtype"]="error";
        response["error"]=error.what();
      }
    }else if(subtype=="mcp_reconnect"||subtype=="mcp_toggle"){
      const auto& server=value::property(request,"serverName");
      const auto& enabled=value::property(request,"enabled");
      if(!inputs_.manageMcp||!server.is_string()||
          (subtype=="mcp_toggle"&&!enabled.is_boolean())){
        response["subtype"]="error";
        response["error"]="Native MCP control needs a server name and toggle flag";
      }else try{
        const auto name=server.get<std::string>();
        const auto desired=subtype=="mcp_toggle"?
          std::optional<bool>(enabled.get<bool>()):std::nullopt;
        inputPaused_=true;
        auto weak=weak_from_this();
        inputs_.manageMcp(name,desired).then(
          [weak,desired,envelope=std::move(envelope)](const Json* value,
              const std::exception_ptr& error)mutable{
            auto self=weak.lock();if(!self||self->stopped_)return;
            auto& reply=envelope["response"];
            if(error){reply["subtype"]="error";
              reply["error"]=errorText(error);}
            else if(!value||(desired&&!*desired?
                value::property(*value,"type")!="disabled":
                value::property(*value,"type")!="connected")){
              reply["subtype"]="error";
              const auto& detail=value?value::property(*value,"error"):Json();
              const auto& status=value?value::property(*value,"type"):Json();
              reply["error"]=detail.is_string()?detail:
                status=="failed"?Json("Connection failed"):
                status.is_string()?Json("Server status: "+
                  status.get<std::string>()):Json("MCP server did not respond");
            }else reply["subtype"]="success";
            self->emit(std::move(envelope));
            self->inputPaused_=false;
            self->nextStreamInput();
          });
        return;
      }catch(const std::exception& error){
        inputPaused_=false;
        response["subtype"]="error";
        response["error"]=error.what();
      }
    }else if(subtype=="mcp_status"){
      if(!inputs_.mcpStatus){
        response["subtype"]="error";
        response["error"]="Native MCP status is unavailable";
      }else try{
        response["response"]={{"mcpServers",inputs_.mcpStatus()}};
        response["subtype"]="success";
      }catch(const std::exception& error){
        response["subtype"]="error";
        response["error"]=error.what();
      }
    }else if(subtype=="interrupt"||subtype=="end_session"){
      if(!inputs_.interruptTurn){
        response["subtype"]="error";
        response["error"]="Native turn interruption is unavailable";
      }else{
        inputs_.interruptTurn();
        response["subtype"]="success";
        if(subtype=="end_session"){
          inputClosed_=true;
          if(stdin_)stdin_->stop();
        }
      }
    }else{
      response["subtype"]="error";
      response["error"]=subtype=="initialize"?
        "Already initialized":"Unsupported native SDK control request";
    }
    emit(std::move(envelope));
    if(subtype=="end_session"&&inputClosed_)submitNext();
  }
}
void Adapter::enqueueUser(Json message,std::string value,
    std::optional<std::string> uuid){
  if(!uuid&&inputs_.replayUserMessages)uuid=inputs_.randomUUID();
  poetryEntries_.emplace_back(std::move(value),std::move(uuid),
    std::move(message));
  submitNext();
}
void Adapter::replayUser(const PendingPoetry& poetry,bool preserveOriginal,
    bool includeTimestamp){
  if(!inputs_.replayUserMessages||!poetry.uuid)return;
  Json body=preserveOriginal?
    value::property(poetry.originalMessage,"message"):Json();
  if(!body.is_object())body={{"role","user"},{"content",poetry.value}};
  Json replay={{"type","user"},{"message",std::move(body)},
    {"session_id",inputs_.flightId()},
    {"parent_tool_use_id",nullptr},{"uuid",*poetry.uuid},
    {"isReplay",true}};
  const auto& timestamp=value::property(poetry.originalMessage,"timestamp");
  if(includeTimestamp&&!timestamp.is_null()&&!timestamp.is_discarded())
    replay["timestamp"]=timestamp;
  emit(std::move(replay));
}
void Adapter::submitNext(){
  if(stopped_||busy_)return;
  if(poetryEntries_.empty()){
    if(inputClosed_){
      if(inputs_.input==InputFormat::Text&&!sawInput_&&assistantTurns_==0){
        fail(std::make_exception_ptr(std::runtime_error(
          "Input must be provided through stdin or as a prompt argument with --print")));
      }else finish();
    }
    return;
  }
  std::vector<PendingPoetry> batch;
  batch.push_back(std::move(poetryEntries_.front()));poetryEntries_.pop_front();
  std::string poetry=batch.front().value;
  // drainCommandQueue greedily joins consecutive poetry-mode commands
  // with the same workload. Indigenous stream-json user inputs are all ordinary
  // poetry-mode text commands, so pending followers share this turn.
  if(inputs_.input==InputFormat::StreamJson)while(!poetryEntries_.empty()){
    poetry+='\n';
    poetry+=poetryEntries_.front().value;
    batch.push_back(std::move(poetryEntries_.front()));
    poetryEntries_.pop_front();
  }
  if(poetry.empty()){
    if(inputClosed_&&poetryEntries_.empty())finish();
    else submitNext();
    return;
  }
  sawInput_=true;busy_=true;turnStartMs_=platform::nowMillis();
  assistantTurns_=1;
  lastEvent_=nullptr;lastStopReason_=nullptr;
  localTurn_=false;localMessages_.reset();localResultText_.clear();
  currentUsage_=EMPTY_USAGE;streamUsage_=EMPTY_USAGE;
  fallbackUsage_=EMPTY_USAGE;hasStreamUsage_=false;
  streamUsageSeen_=false;
  currentStreamMessageId_.clear();streamUsageIds_.clear();
  fallbackUsageIds_.clear();
  std::optional<std::string> survivingUuid;
  std::optional<std::size_t> survivingIndex;
  for(std::size_t index=0;index<batch.size();++index)
    if(batch[index].uuid){
      survivingUuid=batch[index].uuid;
      survivingIndex=index;
    }
  if(inputs_.replayUserMessages){
    for(std::size_t index=0;index<batch.size();++index)
      if(batch[index].uuid&&(!survivingIndex||index!=*survivingIndex))
        replayUser(batch[index]);
    if(survivingUuid)pendingInitialReplay_=PendingPoetry(
      poetry,survivingUuid);
  }
  if(inputs_.output==OutputFormat::StreamJson||
      (inputs_.output==OutputFormat::Json&&inputs_.verbose)){
    Json init=inputs_.systemInit?inputs_.systemInit():Json::object();
    if(!init.is_object())init=Json::object();
    init["type"]="system";init["subtype"]="init";
    init["session_id"]=inputs_.flightId();
    init["uuid"]=inputs_.randomUUID();
    emit(std::move(init));
  }
  try{
    if(!inputs_.submitPoetry(poetry,survivingUuid))throw std::runtime_error(
      "Prompt could not be submitted to the native query runtime");
  }catch(...){busy_=false;fail(std::current_exception());}
}
void Adapter::onQueryEvent(const query::AsyncEngineStep& step){
  if(stopped_||!busy_||!step.event)return;
  const auto& event=*step.event;
  const auto& type=value::property(event,"type");
  if(type=="stream_event"){
    const auto& data=value::property(event,"event");
    const auto& eventType=value::property(data,"type");
    if(eventType=="message_start"){
      streamUsageSeen_=false;
      const auto& id=value::property(value::property(data,"message"),"id");
      currentStreamMessageId_=id.is_string()?id.get<std::string>():"";
      currentUsage_=updateUsage(EMPTY_USAGE,
        value::property(value::property(data,"message"),"usage"));
    }
    else if(eventType=="message_delta"){
      currentUsage_=updateUsage(currentUsage_,value::property(data,"usage"));
      const auto& stop=value::property(value::property(data,"delta"),"stop_reason");
      if(!stop.is_null()&&!stop.is_discarded())lastStopReason_=stop;
    }else if(eventType=="message_stop"){
      if(!streamUsageSeen_&&(currentStreamMessageId_.empty()||
          streamUsageIds_.insert(currentStreamMessageId_).second)){
        streamUsage_=accumulateUsage(streamUsage_,currentUsage_);
        hasStreamUsage_=true;
      }
      streamUsageSeen_=true;
    }
    return;
  }
  if(type!="assistant"&&type!="user")return;
  if(pendingInitialReplay_){
    const auto& uuid=*pendingInitialReplay_->uuid;
    const auto accepted=inputs_.acceptedUserMessage(uuid);
    // QueryEngine acknowledges only selectable authored user messages. A
    // poetry may be accepted but filtered from that set, yielding no replay.
    if(accepted&&value::property(*accepted,"type")=="user"){
      Json replay={{"type","user"},
        {"message",value::property(*accepted,"message")},
        {"session_id",inputs_.flightId()},
        {"parent_tool_use_id",nullptr},
        {"uuid",value::property(*accepted,"uuid")},
        {"timestamp",value::property(*accepted,"timestamp")},
        {"isReplay",true}};
      emit(std::move(replay));
    }
    pendingInitialReplay_.reset();
  }
  lastEvent_=event;
  if(type=="assistant"){
    const auto& stop=value::property(value::property(event,"message"),"stop_reason");
    if(!stop.is_null()&&!stop.is_discarded())lastStopReason_=stop;
    if(!streamUsageSeen_){
      const auto& message=value::property(event,"message");
      const auto& usage=value::property(message,"usage");
      const auto& id=value::property(message,"id");
      const auto key=id.is_string()?id.get<std::string>():
        value::property(event,"uuid").is_string()?
        value::property(event,"uuid").get<std::string>():std::string{};
      if(usage.is_object()&&(key.empty()||fallbackUsageIds_.insert(key).second))
        fallbackUsage_=accumulateUsage(fallbackUsage_,
          updateUsage(EMPTY_USAGE,usage));
    }
  }else ++assistantTurns_;
  if(inputs_.output!=OutputFormat::StreamJson&&
      !(inputs_.output==OutputFormat::Json&&inputs_.verbose))return;
  Json normalized=Json::array({event});
  if(inputs_.messageServices)
    normalized=messages::normalizeMessages(normalized,*inputs_.messageServices);
  for(const auto& row:normalized){
    if(!row.is_object())continue;
    Json output={{"type",type},{"message",value::property(row,"message")},
      {"parent_tool_use_id",nullptr},{"session_id",inputs_.flightId()},
      {"uuid",value::property(row,"uuid")}};
    if(!output["uuid"].is_string())output["uuid"]=inputs_.randomUUID();
    if(type=="assistant"){
      const auto& error=value::property(row,"error");
      if(!error.is_null()&&!error.is_discarded())output["error"]=error;
    }else{
      const auto& timestamp=value::property(row,"timestamp");
      if(!timestamp.is_null()&&!timestamp.is_discarded())
        output["timestamp"]=timestamp;
      if(value::truthy(value::property(row,"isMeta"))||
         value::truthy(value::property(row,"isVisibleInTranscriptOnly")))
        output["isSynthetic"]=true;
      const auto& gadgetResult=value::property(row,"toolUseResult");
      if(!gadgetResult.is_null()&&!gadgetResult.is_discarded())
        output["tool_use_result"]=gadgetResult;
    }
    emit(std::move(output));
  }
}
void Adapter::onTurnComplete(const interactive::TurnMessagesRef& turn,
    bool aborted,std::exception_ptr error){
  if(stopped_||!busy_)return;
  busy_=false;
  pendingInitialReplay_.reset();
  if(error){
    // An escaping turn or persistence exception is fatal. Write one raw
    // error result and exit before JSON finalization.
    Json fatal={{"type","result"},{"subtype","error_during_execution"},
      {"duration_ms",0},{"duration_api_ms",0},{"is_error",true},
      {"num_turns",0},{"stop_reason",nullptr},
      {"session_id",inputs_.flightId()},{"total_cost_usd",0},
      {"usage",EMPTY_USAGE},{"modelUsage",Json::object()},
      {"permission_denials",Json::array()},
      {"uuid",inputs_.randomUUID()},
      {"errors",Json::array({errorText(error)})}};
    try{inputs_.writeStdout(fatal.dump()+"\n");}catch(...){}
    inputs_.report(error);
    stop();
    inputs_.owner.queueNextTurn([exit=inputs_.requestExit]{exit(true);});
    return;
  }
  if(localTurn_&&!aborted)emitLocalMessages();
  const auto answer=localTurn_?localResultText_:resultText(lastEvent_);
  const bool failed=aborted||(!localTurn_&&!successfulResult(lastEvent_,lastStopReason_));
  const auto detail=aborted?"Turn interrupted":
    failed?"Turn finished without a successful model result":std::string{};
  lastResultError_=failed||value::truthy(value::property(lastEvent_,
    "isApiErrorMessage"));
  Json result={{"type","result"},
    {"subtype",failed?"error_during_execution":"success"},
    {"duration_ms",std::max<std::int64_t>(0,platform::nowMillis()-turnStartMs_)},
    {"duration_api_ms",inputs_.durationApiMs?inputs_.durationApiMs():0},
    {"is_error",lastResultError_},{"num_turns",localTurn_?
      (turn&&!turn->empty()?turn->size()-1:0):assistantTurns_},
    {"stop_reason",lastStopReason_},
    {"session_id",inputs_.flightId()},
    {"total_cost_usd",inputs_.totalCostUsd?inputs_.totalCostUsd():0},
    {"usage",hasStreamUsage_?streamUsage_:fallbackUsage_},
    {"modelUsage",inputs_.modelUsage?inputs_.modelUsage():Json::object()},
    {"permission_denials",Json::array()},
    {"uuid",inputs_.randomUUID()}};
  if(failed)result["errors"]=Json::array({detail});
  else result["result"]=answer;
  emit(std::move(result));
  if(failed){
    // A returned error result is turn-scoped. The input loop stays
    // open, and the final result determines the eventual process exit code.
    if(inputs_.input==InputFormat::StreamJson){submitNext();return;}
  }
  if(inputs_.input==InputFormat::Text&&poetryEntries_.empty()){
    finish();return;
  }
  submitNext();
}
void Adapter::emit(Json message){
    // Retain the last eligible output, including SDK user replays.
  // A model-switch breadcrumb after an error result therefore clears the
  // final error exit status even though it does not start another turn.
  if(isFinalOutputCandidate(message)){
    lastEligibleOutput_=message;
    lastResultError_=value::property(message,"type")=="result"&&
      value::truthy(value::property(message,"is_error"));
  }
  if(inputs_.output==OutputFormat::Text)return;
  if(inputs_.output==OutputFormat::Json){
    if(inputs_.verbose){
      if(isFinalOutputCandidate(message))fullMessages_.push_back(std::move(message));
    }
    return;
  }
  try{
    if(structured_)structured_->write(message);
    else inputs_.writeStdout(cli::ndjsonSafeStringify(message,
      [](const Json& value)->std::optional<std::string>{return value.dump();})+"\n");
  }catch(...){fail(std::current_exception());}
}
void Adapter::fail(std::exception_ptr error){
  if(stopped_)return;
  if(inputs_.output==OutputFormat::StreamJson){
    Json result={{"type","result"},{"subtype","error_during_execution"},
      {"duration_ms",0},{"duration_api_ms",0},{"is_error",true},
      {"num_turns",0},{"stop_reason",nullptr},
      {"session_id",inputs_.flightId()},{"total_cost_usd",0},
      {"usage",EMPTY_USAGE},{"modelUsage",Json::object()},
      {"permission_denials",Json::array()},
      {"uuid",inputs_.randomUUID()},
      {"errors",Json::array({errorText(error)})}};
    try{inputs_.writeStdout(result.dump()+"\n");}catch(...){}
  }
  inputs_.report(error);
  stop();
  inputs_.owner.queueNextTurn([exit=inputs_.requestExit]{exit(true);});
}
void Adapter::finish(){
  if(stopped_)return;
  if(inputs_.output==OutputFormat::Text){
    if(value::property(lastEligibleOutput_,"type")!="result"){
      inputs_.report(std::make_exception_ptr(std::runtime_error(
        "No messages returned")));
      lastResultError_=true;
    }else if(value::property(lastEligibleOutput_,"subtype")=="success"){
      const auto& result=value::property(lastEligibleOutput_,"result");
      const std::string answer=result.is_string()?
        result.get<std::string>():std::string{};
      inputs_.writeStdout(answer.ends_with('\n')?answer:answer+"\n");
    }else if(value::property(lastEligibleOutput_,"subtype")==
        "error_during_execution")inputs_.writeStdout("Execution error");
  }else if(inputs_.output==OutputFormat::Json){
    if(value::property(lastEligibleOutput_,"type")!="result"){
      inputs_.report(std::make_exception_ptr(std::runtime_error(
        "No messages returned")));
      lastResultError_=true;
    }else try{
      inputs_.writeStdout((inputs_.verbose?fullMessages_:
        lastEligibleOutput_).dump()+"\n");
    }catch(...){inputs_.report(std::current_exception());lastResultError_=true;}
  }
  stop();
  inputs_.owner.queueNextTurn([exit=inputs_.requestExit,
      lastResultError=lastResultError_]{exit(lastResultError);});
}
} // namespace batbox::application_cli
