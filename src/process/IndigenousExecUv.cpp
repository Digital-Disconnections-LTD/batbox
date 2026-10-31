#include "process/ExecFileNoThrow.hpp"
#include "runtime/AsyncLaunch.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/JsonValue.hpp"
#include <uv.h>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <csignal>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace batbox::process {
namespace {
void check(int code,const char* operation){
  if(code<0)throw std::runtime_error(std::string(operation)+": "+uv_strerror(code));
}
void stripFinalNewline(std::string& value){
  if(!value.empty()&&value.back()=='\n'){
    value.pop_back();if(!value.empty()&&value.back()=='\r')value.pop_back();
  }
}
void validate(const ExecaRequest& request){
  if(request.file.empty())throw std::invalid_argument("Process executable required");
  auto noNul=[](const std::string& value){
    if(value.find('\0')!=std::string::npos)
      throw std::invalid_argument("Process argv, cwd and environment cannot contain NUL");
  };
  noNul(request.file);for(const auto& arg:request.args)noNul(arg);
  if(request.options.cwd)noNul(*request.options.cwd);
  if(request.options.env)for(const auto& [key,value]:*request.options.env){
    noNul(key);noNul(value);
    if(key.find('=')!=std::string::npos)
      throw std::invalid_argument("Process environment key cannot contain equals");
  }
  if(request.options.shell&&value::truthy(*request.options.shell))
    throw std::invalid_argument("Native argv process adapter does not implement shell option");
}
std::vector<std::string> environment(const ExecOptions& options){
  if(!options.env)return {};
  struct KeyLess {
    bool operator()(const std::string& left,const std::string& right)const{
#if defined(_WIN32)
      const auto size=std::min(left.size(),right.size());
      for(std::size_t index=0;index<size;++index){
        const auto a=std::tolower(static_cast<unsigned char>(left[index]));
        const auto b=std::tolower(static_cast<unsigned char>(right[index]));
        if(a!=b)return a<b;
      }
      return left.size()<right.size();
#else
      return left<right;
#endif
    }
  };
  std::map<std::string,std::string,KeyLess> values;
  uv_env_item_t* inherited=nullptr;int count=0;
  check(uv_os_environ(&inherited,&count),"Read process environment");
  for(int index=0;index<count;++index)
    values[inherited[index].name]=inherited[index].value;
  uv_os_free_environ(inherited,count);
  for(const auto& [key,value]:*options.env)
    values[value::encodeUtf8(key)]=value::encodeUtf8(value);
  std::vector<std::string> result;result.reserve(values.size());
  for(const auto& [key,value]:values)result.push_back(key+"="+value);
  return result;
}
struct Run {
  explicit Run(ExecaRequest request):request(std::move(request)){}
  ExecaRequest request;
  uv_loop_t loop{};
  uv_process_t child{};
  uv_pipe_t out{},err{},in{};
  uv_timer_t timer{};
  uv_write_t inputWrite{};
  bool loopReady=false,outReady=false,errReady=false,inReady=false,
    timerReady=false,childReady=false,childExited=false,killed=false,
    timedOut=false,aborted=false,tooMuch=false;
  std::int64_t childStatus=0;
  int childSignal=0;
  std::string inputBytes,stdoutBytes,stderrBytes;
  std::optional<std::chrono::steady_clock::time_point> deadline;
  std::optional<std::size_t> maxBuffer;
  std::exception_ptr ioError;
#if defined(_WIN32)
  HANDLE job=nullptr;
  void attachJob(){
    job=::CreateJobObjectW(nullptr,nullptr);
    if(!job)return;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    if(!::SetInformationJobObject(job,JobObjectExtendedLimitInformation,
        &limits,sizeof(limits))||
        !::AssignProcessToJobObject(job,child.process_handle)){
      ::CloseHandle(job);job=nullptr;
    }
  }
  void closeJob(){if(job){::CloseHandle(job);job=nullptr;}}
#endif
  static void close(uv_handle_t* handle){
    if(handle&&!uv_is_closing(handle))uv_close(handle,nullptr);
  }
  void closeHandles(){
    if(timerReady)close(reinterpret_cast<uv_handle_t*>(&timer));
    if(inReady)close(reinterpret_cast<uv_handle_t*>(&in));
    if(outReady)close(reinterpret_cast<uv_handle_t*>(&out));
    if(errReady)close(reinterpret_cast<uv_handle_t*>(&err));
    if(childReady)close(reinterpret_cast<uv_handle_t*>(&child));
  }
  void drainLoop(){
    if(!loopReady)return;
    while(uv_loop_alive(&loop))uv_run(&loop,UV_RUN_DEFAULT);
    while(uv_loop_close(&loop)==UV_EBUSY)uv_run(&loop,UV_RUN_NOWAIT);
    loopReady=false;
  }
  void stopChild(){
    if(killed||!childReady||childExited)return;
    killed=true;
#if defined(_WIN32)
    if(job)(void)::TerminateJobObject(job,1);
    else (void)uv_process_kill(&child,SIGTERM);
#else
    // The POSIX adapter starts a process group; this libuv fallback asks the
    // child to stop. Descendant cleanup remains a separate platform gate.
    (void)uv_process_kill(&child,SIGKILL);
#endif
    if(inReady)close(reinterpret_cast<uv_handle_t*>(&in));
  }
  static void allocate(uv_handle_t*,std::size_t suggested,uv_buf_t* buffer){
    const auto size=std::max<std::size_t>(suggested,8192);
    buffer->base=new char[size];buffer->len=static_cast<unsigned int>(size);
  }
  void append(std::string& output,const char* data,std::size_t count){
    if(maxBuffer&&count>*maxBuffer-std::min(output.size(),*maxBuffer)){
      const auto available=*maxBuffer>output.size()?*maxBuffer-output.size():0;
      output.append(data,std::min(count,available));tooMuch=true;
    }else output.append(data,count);
  }
  static void read(uv_stream_t* stream,std::ptrdiff_t count,const uv_buf_t* buffer){
    std::unique_ptr<char[]> bytes(buffer->base);
    auto* self=static_cast<Run*>(stream->data);
    if(count>0){
      auto& output=stream==reinterpret_cast<uv_stream_t*>(&self->out)?
        self->stdoutBytes:self->stderrBytes;
      self->append(output,buffer->base,static_cast<std::size_t>(count));
    }else if(count<0){
      if(count!=UV_EOF)self->ioError=std::make_exception_ptr(
        std::runtime_error(std::string("Process output read failed: ")+
          uv_strerror(static_cast<int>(count))));
      close(reinterpret_cast<uv_handle_t*>(stream));
    }
  }
  static void wrote(uv_write_t* request,int){
    auto* self=static_cast<Run*>(request->data);
    if(self->inReady)close(reinterpret_cast<uv_handle_t*>(&self->in));
  }
  static void exited(uv_process_t* process,std::int64_t status,int signal){
    auto* self=static_cast<Run*>(process->data);
    self->childExited=true;self->childStatus=status;self->childSignal=signal;
    if(self->timerReady)close(reinterpret_cast<uv_handle_t*>(&self->timer));
    if(self->inReady)close(reinterpret_cast<uv_handle_t*>(&self->in));
    close(reinterpret_cast<uv_handle_t*>(process));
  }
  static void tick(uv_timer_t* timer){
    auto* self=static_cast<Run*>(timer->data);
    if(self->killed||self->childExited)return;
    self->aborted=self->request.options.abortSignal.stop_requested();
    self->timedOut=!self->aborted&&self->deadline&&
      std::chrono::steady_clock::now()>=*self->deadline;
    if(self->aborted||self->timedOut||self->tooMuch||self->ioError)
      self->stopChild();
  }
  ExecaResult execute(){
    validate(request);
    check(uv_loop_init(&loop),"Initialize process loop");loopReady=true;
    try{
      check(uv_pipe_init(&loop,&out,0),"Initialize stdout pipe");outReady=true;
      check(uv_pipe_init(&loop,&err,0),"Initialize stderr pipe");errReady=true;
      const bool inputPipe=request.options.input.has_value()||
        request.options.stdin=="pipe";
      if(inputPipe){check(uv_pipe_init(&loop,&in,0),
        "Initialize stdin pipe");inReady=true;}
      check(uv_timer_init(&loop,&timer),"Initialize process timer");timerReady=true;
      out.data=this;err.data=this;timer.data=this;
      if(inReady)in.data=this;
      if(request.options.maxBuffer&&
          std::isfinite(*request.options.maxBuffer)&&
          *request.options.maxBuffer>=0)
        maxBuffer=static_cast<std::size_t>(std::min(*request.options.maxBuffer,
          static_cast<double>(std::numeric_limits<std::size_t>::max())));
      if(request.options.timeout&&std::isfinite(*request.options.timeout)&&
          *request.options.timeout>0){
        const auto millis=static_cast<std::int64_t>(std::min(*request.options.timeout,
          static_cast<double>(std::chrono::hours(24*365).count()*3600000LL)));
        deadline=std::chrono::steady_clock::now()+
          std::chrono::milliseconds(millis);
      }
      const auto file=value::encodeUtf8(request.file);
      std::vector<std::string> arguments{file};
      for(const auto& arg:request.args)arguments.push_back(value::encodeUtf8(arg));
      std::vector<char*> argv;argv.reserve(arguments.size()+1);
      for(auto& arg:arguments)argv.push_back(arg.data());
      argv.push_back(nullptr);
      auto env=environment(request.options);
      std::vector<char*> envp;envp.reserve(env.size()+1);
      for(auto& item:env)envp.push_back(item.data());
      envp.push_back(nullptr);
      auto cwd=request.options.cwd?value::encodeUtf8(*request.options.cwd):std::string{};
      uv_stdio_container_t stdio[3]{};
      if(inReady){stdio[0].flags=static_cast<uv_stdio_flags>(
          UV_CREATE_PIPE|UV_READABLE_PIPE);stdio[0].data.stream=
          reinterpret_cast<uv_stream_t*>(&in);}
      else if(request.options.stdin=="inherit"){
        stdio[0].flags=UV_INHERIT_FD;stdio[0].data.fd=0;
      }else stdio[0].flags=UV_IGNORE;
      stdio[1].flags=static_cast<uv_stdio_flags>(UV_CREATE_PIPE|UV_WRITABLE_PIPE);
      stdio[1].data.stream=reinterpret_cast<uv_stream_t*>(&out);
      stdio[2].flags=static_cast<uv_stdio_flags>(UV_CREATE_PIPE|UV_WRITABLE_PIPE);
      stdio[2].data.stream=reinterpret_cast<uv_stream_t*>(&err);
      uv_process_options_t options{};
      options.exit_cb=exited;options.file=file.c_str();options.args=argv.data();
      options.env=request.options.env?envp.data():nullptr;
      options.cwd=cwd.empty()?nullptr:cwd.c_str();
      options.stdio_count=3;options.stdio=stdio;
#if defined(_WIN32)
      options.flags=UV_PROCESS_WINDOWS_HIDE;
      if(request.options.windowsVerbatimArguments)
        options.flags|=UV_PROCESS_WINDOWS_VERBATIM_ARGUMENTS;
#endif
      child.data=this;
      const int spawned=uv_spawn(&loop,&child,&options);
      if(spawned<0){
        // uv_spawn initializes this handle on failure too. Leaving it open
        // prevents drainLoop from closing the private event loop.
        close(reinterpret_cast<uv_handle_t*>(&child));
        closeHandles();drainLoop();
        return ExecaResult{true,{},"","",
          std::string("Command failed to start: ")+uv_strerror(spawned),{}};
      }
      childReady=true;
#if defined(_WIN32)
      attachJob();
#endif
      check(uv_read_start(reinterpret_cast<uv_stream_t*>(&out),allocate,read),
        "Start stdout read");
      check(uv_read_start(reinterpret_cast<uv_stream_t*>(&err),allocate,read),
        "Start stderr read");
      if(inReady){
        inputBytes=request.options.input?
          value::encodeUtf8(*request.options.input):std::string{};
        if(inputBytes.empty())close(reinterpret_cast<uv_handle_t*>(&in));
        else{
          if(inputBytes.size()>std::numeric_limits<unsigned int>::max())
            throw std::length_error("Process stdin exceeds native write size");
          inputWrite.data=this;
          uv_buf_t buffer=uv_buf_init(inputBytes.data(),
            static_cast<unsigned int>(inputBytes.size()));
          check(uv_write(&inputWrite,reinterpret_cast<uv_stream_t*>(&in),
            &buffer,1,wrote),"Write process stdin");
        }
      }
      check(uv_timer_start(&timer,tick,25,25),"Start process timeout");
      drainLoop();
#if defined(_WIN32)
      closeJob();
#endif
      if(ioError)std::rethrow_exception(ioError);
      ExecaResult result;
      result.stdoutText=value::decodeUtf8(stdoutBytes);
      result.stderrText=value::decodeUtf8(stderrBytes);
      stripFinalNewline(result.stdoutText);stripFinalNewline(result.stderrText);
      if(childSignal==0&&childStatus>=std::numeric_limits<int>::min()&&
          childStatus<=std::numeric_limits<int>::max())
        result.exitCode=static_cast<int>(childStatus);
      if(timedOut)result.shortMessage="Command timed out";
      else if(aborted)result.shortMessage="Command was aborted";
      else if(tooMuch)result.shortMessage="Command output exceeded maxBuffer";
      else if(childSignal)result.signal="SIG"+std::to_string(childSignal);
      else if(result.exitCode&&*result.exitCode!=0)
        result.shortMessage="Command failed with exit code "+
          std::to_string(*result.exitCode);
      result.failed=timedOut||aborted||tooMuch||!result.exitCode||
        *result.exitCode!=0;
      return result;
    }catch(...){
      const auto error=std::current_exception();
      stopChild();closeHandles();drainLoop();
#if defined(_WIN32)
      closeJob();
#endif
      std::rethrow_exception(error);
    }
  }
};
}

std::function<event_loop::Deferred<ExecaResult>(ExecaRequest)> indigenousExeca(
    event_loop::Loop& owner){
  return [&owner](ExecaRequest request){
    if(!owner.isOwnerThread())throw std::logic_error("native execa owner required");
    event_loop::Deferred<ExecaResult> result(owner);
    auto keep=std::make_shared<event_loop::Loop::KeepAlive>(owner.hold());
    const auto dispatcher=owner.dispatcher();
    try{
      runtime::contextThread([result,request=std::move(request),keep,
          dispatcher]()mutable{
        std::optional<ExecaResult> value;std::exception_ptr error;
        try{value=Run(std::move(request)).execute();}
        catch(...){error=std::current_exception();}
        try{dispatcher.postMission([result,value=std::move(value),error,keep]{
          if(error)result.reject(error);else result.resolve(*value);
          keep->release();});}
        catch(...){keep->release();}
      }).detach();
    }catch(...){keep->release();result.reject(std::current_exception());}
    return result;
  };
}
} // namespace batbox::process
