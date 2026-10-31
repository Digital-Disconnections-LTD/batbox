#include "process/IndigenousJsonlProcess.hpp"
#include "connectors/Abort.hpp"
#include "runtime/JsonValue.hpp"
#include <uv.h>
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cctype>
#include <csignal>
#include <deque>
#include <future>
#include <map>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace batbox::process {
namespace {
constexpr std::size_t maxDiagnosticBytes=64*1024;
std::vector<std::string> environment(const std::map<std::string,std::string>& overrides,
    bool inherit){
  struct KeyLess {bool operator()(const std::string& left,const std::string& right)const{
#ifdef _WIN32
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
  }};
  std::map<std::string,std::string,KeyLess> values;
  if(inherit){
    uv_env_item_t* inherited=nullptr;int count=0;
    const auto code=uv_os_environ(&inherited,&count);
    if(code<0)throw std::runtime_error(std::string("Read child environment: ")+uv_strerror(code));
    for(int index=0;index<count;++index)values[inherited[index].name]=inherited[index].value;
    uv_os_free_environ(inherited,count);
  }
  for(const auto& [key,value]:overrides){
    if(key.empty()||key.find('=')!=std::string::npos||
       key.find('\0')!=std::string::npos||value.find('\0')!=std::string::npos)
      throw std::invalid_argument("Invalid child environment override");
    values[key]=value;
  }
  std::vector<std::string> result;result.reserve(values.size());
  for(const auto& [key,value]:values)result.push_back(key+"="+value);
  return result;
}
}
struct IndigenousJsonlProcess::Impl {
  struct Write {
    uv_write_t request{};
    std::string bytes;
    std::mutex mutex;
    std::condition_variable done;
    bool complete=false;
    std::exception_ptr error;
  };
  std::vector<std::string> command;
  std::map<std::string,std::string> overrides;
  IndigenousChildOptions options;
  uv_loop_t loop{};
  uv_process_t child{};
  uv_pipe_t input{},output{},diagnostic{};
  uv_async_t wake{};
  uv_timer_t killTimer{};
  bool loopReady=false,childReady=false,inputReady=false,outputReady=false,
    diagnosticReady=false,wakeReady=false,timerReady=false,childExited=false;
  std::atomic<bool> stdinCloseRequested{false},killRequested{false};
#ifdef _WIN32
  HANDLE group=nullptr;
  void attachGroup(){
    group=::CreateJobObjectW(nullptr,nullptr);
    if(!group)return;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    HANDLE process=::OpenProcess(PROCESS_SET_QUOTA|PROCESS_TERMINATE|PROCESS_QUERY_INFORMATION,
      FALSE,child.pid);
    const bool assigned=process&&::SetInformationJobObject(group,
      JobObjectExtendedLimitInformation,&limits,sizeof(limits))&&
      ::AssignProcessToJobObject(group,process);
    if(process)::CloseHandle(process);
    if(!assigned){::CloseHandle(group);group=nullptr;}
  }
#endif
  void killChild(int signal){
    if(!childReady||childExited)return;
#ifdef _WIN32
    if(group){::TerminateJobObject(group,signal==SIGTERM?143:137);return;}
#endif
    uv_process_kill(&child,signal);
  }
  std::atomic<bool> closing{false},running{false};
  std::promise<void> started;
  std::thread thread;
  mutable std::mutex mutex;
  std::condition_variable ready;
  std::deque<std::string> lines;
  std::string buffered,stderrBytes;
  bool outputEnded=false,diagnosticEnded=false,stderrWasTruncated=false;
  bool exitObserved=false;
  int exitCode=1;
  long processId=0;
  std::deque<std::shared_ptr<Write>> queued;
  std::map<uv_write_t*,std::shared_ptr<Write>> active;
  explicit Impl(std::vector<std::string> argv,std::map<std::string,std::string> env,
      IndigenousChildOptions childOptions):
    command(std::move(argv)),overrides(std::move(env)),options(std::move(childOptions)){}
  static void closeHandle(uv_handle_t* handle){if(!uv_is_closing(handle))uv_close(handle,nullptr);}
  static void allocation(uv_handle_t*,std::size_t size,uv_buf_t* buffer){
    buffer->base=new char[size];buffer->len=static_cast<unsigned int>(size);
  }
  void endOutput(){
    {std::lock_guard lock(mutex);outputEnded=true;}
    ready.notify_all();
  }
  static void outputRead(uv_stream_t* stream,ssize_t count,const uv_buf_t* buffer){
    auto* self=static_cast<Impl*>(stream->data);
    if(count>0){
      if(self->options.onData)try{
        self->options.onData(std::string(buffer->base,static_cast<std::size_t>(count)),true);
      }catch(...){}
      if(self->options.lineMode){
        std::lock_guard lock(self->mutex);
        self->buffered.append(buffer->base,static_cast<std::size_t>(count));
        std::size_t newline;
        while((newline=self->buffered.find('\n'))!=std::string::npos){
          self->lines.push_back(self->buffered.substr(0,newline));
          self->buffered.erase(0,newline+1);
        }
        self->ready.notify_all();
      }
    }
    delete[] buffer->base;
    if(count<0){self->endOutput();closeHandle(reinterpret_cast<uv_handle_t*>(stream));}
  }
  static void diagnosticRead(uv_stream_t* stream,ssize_t count,const uv_buf_t* buffer){
    auto* self=static_cast<Impl*>(stream->data);
    if(count>0){
      if(self->options.onData)try{
        self->options.onData(std::string(buffer->base,static_cast<std::size_t>(count)),false);
      }catch(...){}
      std::lock_guard lock(self->mutex);
      self->stderrBytes.append(buffer->base,static_cast<std::size_t>(count));
      if(self->stderrBytes.size()>maxDiagnosticBytes){
        self->stderrBytes.erase(0,self->stderrBytes.size()-maxDiagnosticBytes);
        self->stderrWasTruncated=true;
      }
    }
    delete[] buffer->base;
    if(count<0){
      {std::lock_guard lock(self->mutex);self->diagnosticEnded=true;}
      self->ready.notify_all();
      closeHandle(reinterpret_cast<uv_handle_t*>(stream));
    }
  }
  static void writeDone(uv_write_t* request,int code){
    auto* self=static_cast<Impl*>(request->data);
    auto found=self->active.find(request);
    if(found==self->active.end())return;
    auto write=std::move(found->second);self->active.erase(found);
    {std::lock_guard lock(write->mutex);
      if(code<0)write->error=std::make_exception_ptr(std::runtime_error(
        std::string("Child stdin write failed: ")+uv_strerror(code)));
      write->complete=true;}
    write->done.notify_all();
  }
  static void killExpired(uv_timer_t* timer){
    auto* self=static_cast<Impl*>(timer->data);
    self->killChild(SIGKILL);
  }
  static void processExit(uv_process_t* child,std::int64_t status,int signal){
    auto* self=static_cast<Impl*>(child->data);self->childExited=true;
    {std::lock_guard lock(self->mutex);
      self->exitObserved=true;
      self->exitCode=signal?1:static_cast<int>(status);}
    self->ready.notify_all();
    closeHandle(reinterpret_cast<uv_handle_t*>(child));
#ifdef _WIN32
    if(self->group){::CloseHandle(self->group);self->group=nullptr;}
#endif
    if(self->timerReady)closeHandle(reinterpret_cast<uv_handle_t*>(&self->killTimer));
    if(self->closing){
      if(self->outputReady)closeHandle(reinterpret_cast<uv_handle_t*>(&self->output));
      if(self->diagnosticReady)closeHandle(reinterpret_cast<uv_handle_t*>(&self->diagnostic));
      self->endOutput();
    }
  }
  static void awakened(uv_async_t* async){
    auto* self=static_cast<Impl*>(async->data);
    std::deque<std::shared_ptr<Write>> queued;
    {std::lock_guard lock(self->mutex);queued.swap(self->queued);}
    for(auto& write:queued){
      if(self->closing){
        {std::lock_guard lock(write->mutex);
          write->error=std::make_exception_ptr(std::runtime_error("Child process closed"));
          write->complete=true;}write->done.notify_all();continue;
      }
      auto buffer=uv_buf_init(write->bytes.data(),static_cast<unsigned int>(write->bytes.size()));
      write->request.data=self;self->active[&write->request]=write;
      const auto code=uv_write(&write->request,reinterpret_cast<uv_stream_t*>(&self->input),
        &buffer,1,writeDone);
      if(code<0)writeDone(&write->request,code);
    }
    if(self->killRequested)self->killChild(SIGKILL);
    if(self->stdinCloseRequested&&self->inputReady)
      closeHandle(reinterpret_cast<uv_handle_t*>(&self->input));
    if(!self->closing)return;
    if(self->childReady&&!self->childExited){
      self->killChild(SIGTERM);
      if(self->timerReady)uv_timer_start(&self->killTimer,killExpired,500,0);
    }else if(self->timerReady)closeHandle(reinterpret_cast<uv_handle_t*>(&self->killTimer));
    if(self->inputReady)closeHandle(reinterpret_cast<uv_handle_t*>(&self->input));
    closeHandle(reinterpret_cast<uv_handle_t*>(&self->wake));
    self->endOutput();
  }
  void run(){
    try{
      if(command.empty())throw std::invalid_argument("Child executable required");
      for(const auto& arg:command)if(arg.find('\0')!=std::string::npos)
        throw std::invalid_argument("Child argument contains NUL");
      auto envText=environment(overrides,options.inheritEnvironment);
      if(uv_loop_init(&loop)<0)throw std::runtime_error("Initialize child event loop failed");
      loopReady=true;
      auto initPipe=[this](uv_pipe_t& pipe,bool& readyFlag){
        const auto code=uv_pipe_init(&loop,&pipe,0);
        if(code<0)throw std::runtime_error(uv_strerror(code));
        readyFlag=true;pipe.data=this;
      };
      initPipe(input,inputReady);initPipe(output,outputReady);
      initPipe(diagnostic,diagnosticReady);
      if(uv_async_init(&loop,&wake,awakened)<0)throw std::runtime_error("Initialize child wakeup failed");
      wakeReady=true;wake.data=this;
      if(uv_timer_init(&loop,&killTimer)<0)throw std::runtime_error("Initialize child kill timer failed");
      timerReady=true;killTimer.data=this;
      std::vector<char*> argv;argv.reserve(command.size()+1);
      for(auto& arg:command)argv.push_back(arg.data());
      argv.push_back(nullptr);
      std::vector<char*> envp;envp.reserve(envText.size()+1);
      for(auto& item:envText)envp.push_back(item.data());
      envp.push_back(nullptr);
      uv_stdio_container_t stdio[3]{};
      stdio[0].flags=static_cast<uv_stdio_flags>(UV_CREATE_PIPE|UV_READABLE_PIPE);
      stdio[0].data.stream=reinterpret_cast<uv_stream_t*>(&input);
      stdio[1].flags=static_cast<uv_stdio_flags>(UV_CREATE_PIPE|UV_WRITABLE_PIPE);
      stdio[1].data.stream=reinterpret_cast<uv_stream_t*>(&output);
      stdio[2].flags=static_cast<uv_stdio_flags>(UV_CREATE_PIPE|UV_WRITABLE_PIPE);
      stdio[2].data.stream=reinterpret_cast<uv_stream_t*>(&diagnostic);
      uv_process_options_t options{};options.file=argv[0];options.args=argv.data();
      options.env=envp.data();options.stdio_count=3;options.stdio=stdio;
      options.exit_cb=processExit;
      if(this->options.cwd)options.cwd=this->options.cwd->c_str();
#ifdef _WIN32
      options.flags=UV_PROCESS_WINDOWS_HIDE;
#endif
      const auto spawn=uv_spawn(&loop,&child,&options);
      if(spawn<0){
        // uv_spawn initializes the handle even when no child was started.
        // Drain its close with the other initialized handles below.
        closeHandle(reinterpret_cast<uv_handle_t*>(&child));
        throw IndigenousChildSpawnError(spawn,
          std::string("Cannot start child: ")+uv_strerror(spawn));
      }
      childReady=true;child.data=this;
      processId=static_cast<long>(child.pid);
#ifdef _WIN32
      attachGroup();
#endif
      // libuv reads may fire before the caller starts waiting for lines.
      if(uv_read_start(reinterpret_cast<uv_stream_t*>(&output),allocation,outputRead)<0||
         uv_read_start(reinterpret_cast<uv_stream_t*>(&diagnostic),allocation,diagnosticRead)<0)
        throw std::runtime_error("Start child output reader failed");
      running=true;started.set_value();
      uv_run(&loop,UV_RUN_DEFAULT);
      running=false;endOutput();
      (void)uv_loop_close(&loop);
    }catch(...){
      try{started.set_exception(std::current_exception());}catch(...){}
      closing=true;
      killChild(SIGKILL);
      if(inputReady)closeHandle(reinterpret_cast<uv_handle_t*>(&input));
      if(outputReady)closeHandle(reinterpret_cast<uv_handle_t*>(&output));
      if(diagnosticReady)closeHandle(reinterpret_cast<uv_handle_t*>(&diagnostic));
      if(wakeReady)closeHandle(reinterpret_cast<uv_handle_t*>(&wake));
      if(timerReady)closeHandle(reinterpret_cast<uv_handle_t*>(&killTimer));
      if(loopReady){uv_run(&loop,UV_RUN_DEFAULT);(void)uv_loop_close(&loop);}
      running=false;endOutput();
    }
  }
};
IndigenousJsonlProcess::IndigenousJsonlProcess(const std::vector<std::string>& command,
    const std::map<std::string,std::string>& overrides,IndigenousChildOptions options):
  impl_(std::make_unique<Impl>(command,overrides,std::move(options))){
  auto ready=impl_->started.get_future();
  impl_->thread=std::thread([state=impl_.get()]{state->run();});
  try{ready.get();}catch(...){close();throw;}
}
IndigenousJsonlProcess::~IndigenousJsonlProcess(){close();}
void IndigenousJsonlProcess::writeLine(const std::string& bytes,
    std::chrono::steady_clock::time_point deadline,std::stop_token signal){
  checkConnectorAbort(signal);
  if(bytes.size()>std::numeric_limits<unsigned int>::max())
    throw std::invalid_argument("Child stdin write exceeds libuv buffer limit");
  if(impl_->closing||!impl_->running)throw std::runtime_error("Child process disconnected");
  auto write=std::make_shared<Impl::Write>();write->bytes=bytes;
  {std::lock_guard lock(impl_->mutex);impl_->queued.push_back(write);}
  uv_async_send(&impl_->wake);
  while(true){
    std::unique_lock lock(write->mutex);
    if(write->done.wait_for(lock,std::chrono::milliseconds(50),[&]{return write->complete;})){
      if(write->error)std::rethrow_exception(write->error);
      return;
    }
    lock.unlock();
    if(impl_->closing)throw std::runtime_error("Child process disconnected");
    if(signal.stop_requested()||std::chrono::steady_clock::now()>=deadline){
      close();
      if(signal.stop_requested())checkConnectorAbort(signal);
      throw std::runtime_error("Child stdin write timed out");
    }
  }
}
std::string IndigenousJsonlProcess::readLine(std::chrono::steady_clock::time_point deadline,
    std::stop_token signal){
  if(!impl_->options.lineMode)throw std::logic_error("Child line reads disabled");
  while(true){
    std::unique_lock lock(impl_->mutex);
    if(!impl_->lines.empty()){
      auto line=std::move(impl_->lines.front());impl_->lines.pop_front();return line;
    }
    if(impl_->outputEnded||impl_->closing)throw std::runtime_error("Child process output ended");
    lock.unlock();checkConnectorAbort(signal);
    if(std::chrono::steady_clock::now()>=deadline)
      throw std::runtime_error("Child process read timed out");
    lock.lock();impl_->ready.wait_for(lock,std::chrono::milliseconds(50));
  }
}
std::string IndigenousJsonlProcess::stderrTail()const{
  std::lock_guard lock(impl_->mutex);return impl_->stderrBytes;
}
bool IndigenousJsonlProcess::stderrTruncated()const{
  std::lock_guard lock(impl_->mutex);return impl_->stderrWasTruncated;
}
void IndigenousJsonlProcess::closeStdin(){
  if(impl_->closing||!impl_->running)return;
  impl_->stdinCloseRequested=true;uv_async_send(&impl_->wake);
}
void IndigenousJsonlProcess::kill()noexcept{
  if(impl_->closing||!impl_->running)return;
  impl_->killRequested=true;uv_async_send(&impl_->wake);
}
int IndigenousJsonlProcess::waitForExit(std::chrono::steady_clock::time_point deadline,
    std::stop_token signal){
  while(true){
    std::unique_lock lock(impl_->mutex);
    if(impl_->exitObserved&&impl_->outputEnded&&impl_->diagnosticEnded)
      return impl_->exitCode;
    if(impl_->closing)throw std::runtime_error("Child process closed");
    lock.unlock();checkConnectorAbort(signal);
    if(std::chrono::steady_clock::now()>=deadline)
      throw std::runtime_error("Child process wait timed out");
    lock.lock();impl_->ready.wait_for(lock,std::chrono::milliseconds(50));
  }
}
long IndigenousJsonlProcess::pid()const{return impl_->processId;}
void IndigenousJsonlProcess::close()noexcept{
  if(!impl_)return;
  impl_->closing=true;impl_->ready.notify_all();
  if(impl_->running&&impl_->wakeReady)uv_async_send(&impl_->wake);
  if(impl_->thread.joinable())impl_->thread.join();
}
} // namespace batbox::process
