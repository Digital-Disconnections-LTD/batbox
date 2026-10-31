#include "connectors/Scrapling.hpp"
#include "process/IndigenousJsonlProcess.hpp"
#include <cerrno>
#include <climits>
#include <cstring>
#ifndef _WIN32
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
#include <stdexcept>
#include <thread>
#ifndef _WIN32
extern char** environ;
#endif
namespace batbox {
namespace {
constexpr std::size_t maxDiagnosticBytes=64*1024;
bool environmentAssignment(const std::string& line){
  std::size_t index=0;
  while(index<line.size()&&(line[index]==' '||line[index]=='\t'))++index;
  const auto start=index;
  if(index==line.size()||!(line[index]=='_'||
      (line[index]>='A'&&line[index]<='Z')))return false;
  for(;index<line.size();++index){
    const char c=line[index];
    if(c=='=')return index>start;
    if(!(c=='_'||(c>='A'&&c<='Z')||(c>='0'&&c<='9')))return false;
  }
  return false;
}
#ifndef _WIN32
#if !defined(__linux__)
bool closeOnExec(int fd){const auto flags=::fcntl(fd,F_GETFD);
  return flags>=0&&::fcntl(fd,F_SETFD,flags|FD_CLOEXEC)>=0;}
#endif
bool indigenousPipe(int (&fds)[2]){
#if defined(__linux__)
  return ::pipe2(fds,O_CLOEXEC)==0;
#else
  if(::pipe(fds)!=0)return false;
  if(closeOnExec(fds[0])&&closeOnExec(fds[1]))return true;
  ::close(fds[0]);::close(fds[1]);fds[0]=fds[1]=-1;return false;
#endif
}
bool indigenousSocketPair(int (&fds)[2]){
#if defined(__linux__)
  return ::socketpair(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0,fds)==0;
#else
  if(::socketpair(AF_UNIX,SOCK_STREAM,0,fds)!=0)return false;
  if(closeOnExec(fds[0])&&closeOnExec(fds[1]))return true;
  ::close(fds[0]);::close(fds[1]);fds[0]=fds[1]=-1;return false;
#endif
}
void ready(int fd, short event, std::chrono::steady_clock::time_point deadline,std::stop_token signal) {
  while (true) {
    checkConnectorAbort(signal);
    if(fd<0)throw std::runtime_error("Scrapling process disconnected");
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
    if (remaining <= 0) throw std::runtime_error("Scrapling request timed out");
    pollfd p{fd, event, 0};
    const int result = ::poll(&p, 1, static_cast<int>(std::min<long long>(remaining,signal.stop_possible()?50:INT_MAX)));
    if (result < 0 && errno == EINTR) continue;
    if (result < 0) throw std::runtime_error(std::string("Scrapling poll failed: ") + std::strerror(errno));
    if (!result) continue;
    if (p.revents & event) return;
    throw std::runtime_error("Scrapling process disconnected");
  }
}
#endif
} // namespace
Scrapling::Scrapling(const std::vector<std::string>& command, std::chrono::milliseconds wait) : timeout(wait) {
  if (command.empty()) throw std::invalid_argument("Set a Scrapling MCP server executable");
#ifdef _WIN32
  indigenousProcess=std::make_unique<process::IndigenousJsonlProcess>(command);
#else
  int toChild[2], fromChild[2], errChild[2];
  if (!indigenousSocketPair(toChild)) throw std::runtime_error("Cannot create Scrapling input channel");
  if (!indigenousPipe(fromChild)) { ::close(toChild[0]); ::close(toChild[1]); throw std::runtime_error("Cannot create Scrapling output pipe"); }
  if (!indigenousPipe(errChild)) {
    ::close(toChild[0]); ::close(toChild[1]);
    ::close(fromChild[0]); ::close(fromChild[1]);
    throw std::runtime_error("Cannot create Scrapling diagnostic pipe");
  }
  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_adddup2(&actions, toChild[0], STDIN_FILENO);
  posix_spawn_file_actions_adddup2(&actions, fromChild[1], STDOUT_FILENO);
  posix_spawn_file_actions_adddup2(&actions, errChild[1], STDERR_FILENO);
  for (auto fd : {toChild[0], toChild[1], fromChild[0], fromChild[1],
      errChild[0], errChild[1]}) posix_spawn_file_actions_addclose(&actions, fd);
  posix_spawnattr_t attributes;
  posix_spawnattr_init(&attributes);
  posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
  posix_spawnattr_setpgroup(&attributes, 0);
  std::vector<char*> argv;
  for (const auto& arg : command) argv.push_back(const_cast<char*>(arg.c_str()));
  argv.push_back(nullptr);
  const auto error = posix_spawnp(&child, argv[0], &actions, &attributes, argv.data(), environ);
  posix_spawn_file_actions_destroy(&actions);
  posix_spawnattr_destroy(&attributes);
  ::close(toChild[0]); ::close(fromChild[1]); ::close(errChild[1]);
  input = toChild[1]; output = fromChild[0]; diagnosticOutput=errChild[0];
  if (error) { child = -1; close(); throw std::runtime_error(std::string("Cannot start Scrapling: ")+std::strerror(error)); }
  ::fcntl(input, F_SETFL, ::fcntl(input, F_GETFL) | O_NONBLOCK);
#if defined(__APPLE__)
  int noSigpipe=1;::setsockopt(input,SOL_SOCKET,SO_NOSIGPIPE,&noSigpipe,sizeof(noSigpipe));
#endif
  ::fcntl(output, F_SETFL, ::fcntl(output, F_GETFL) | O_NONBLOCK);
  diagnosticReader=std::thread([this]{readDiagnostics();});
#endif
  try {
    auto initialized = request("initialize", {{"protocolVersion", "2025-03-26"}, {"capabilities", Json::object()},
      {"clientInfo", {{"name", "batbox"}, {"version", "0.0.1"}}}});
    if (!initialized.contains("protocolVersion")) throw std::runtime_error("Scrapling initialization returned no protocol version");
    send({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}}, std::chrono::steady_clock::now()+timeout);
  } catch(const std::exception& error) {
    const std::string reason=error.what();
    close();
    const auto diagnostic=stderrDiagnostic();
    if(!diagnostic.empty())throw std::runtime_error(reason+"; server stderr: "+diagnostic);
    throw std::runtime_error(reason);
  } catch (...) { close(); throw; }
}
Scrapling::~Scrapling() { close(); }
void Scrapling::readDiagnostics(){
#ifndef _WIN32
  for(;;){
    pollfd item{diagnosticOutput,POLLIN,0};
    const auto ready=::poll(&item,1,100);
    if(ready<0&&errno==EINTR)continue;
    if(ready<=0){if(stopDiagnostic||ready<0)break;continue;}
    char bytes[4096];
    const auto count=::read(diagnosticOutput,bytes,sizeof(bytes));
    if(count<0&&errno==EINTR)continue;
    if(count<=0)break;
    std::lock_guard lock(diagnosticMutex);
    diagnosticTail.append(bytes,static_cast<std::size_t>(count));
    if(diagnosticTail.size()>maxDiagnosticBytes){
      diagnosticTail.erase(0,diagnosticTail.size()-maxDiagnosticBytes);
      diagnosticTruncated=true;
    }
  }
#endif
}
std::string Scrapling::stderrDiagnostic()const{
  std::string raw;bool truncated;
#ifdef _WIN32
  if(indigenousProcess){raw=indigenousProcess->stderrTail();truncated=indigenousProcess->stderrTruncated();}
  else truncated=false;
#else
  {std::lock_guard lock(diagnosticMutex);
    raw=diagnosticTail;truncated=diagnosticTruncated;}
#endif
  std::string clean;
  for(unsigned char c:raw){
    if(c=='\n'||c=='\t')clean.push_back(static_cast<char>(c));
    else if(c=='\r')clean.push_back('\n');
    else if(c>=0x20&&c!=0x7f)clean.push_back(static_cast<char>(c));
  }
  std::string result;
  std::size_t start=0;
  while(start<clean.size()){
    const auto end=clean.find('\n',start);
    const auto line=clean.substr(start,end==std::string::npos?std::string::npos:end-start);
    result+=environmentAssignment(line)?"[environment assignment redacted]":line;
    if(end==std::string::npos)break;
    result+='\n';start=end+1;
  }
  if(truncated)result="[earlier stderr truncated]\n"+result;
  return value::decodeUtf8(result);
}
void Scrapling::close() noexcept {
#ifdef _WIN32
  if(indigenousProcess)indigenousProcess->close();
#else
  if (input >= 0) { ::close(input); input = -1; }
  if (output >= 0) { ::close(output); output = -1; }
  if (child > 0 && ::waitpid(child, nullptr, WNOHANG) == 0) {
    ::kill(-child, SIGTERM);
    bool finished = false;
    for (int i = 0; i < 50; ++i) {
      const auto status = ::waitpid(child, nullptr, WNOHANG);
      if (status == child || (status < 0 && errno == ECHILD)) { finished = true; break; }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!finished) { ::kill(-child, SIGKILL); while (::waitpid(child,nullptr,0) < 0 && errno == EINTR) {} }
  }
  child = -1;
  stopDiagnostic=true;
  if(diagnosticReader.joinable())diagnosticReader.join();
  if(diagnosticOutput>=0){::close(diagnosticOutput);diagnosticOutput=-1;}
#endif
}
void Scrapling::send(const Json& message, std::chrono::steady_clock::time_point deadline,std::stop_token signal) {
  auto line = value::stringify(message) + '\n';
#ifdef _WIN32
  if(!indigenousProcess)throw std::runtime_error("Scrapling process disconnected");
  try{indigenousProcess->writeLine(line,deadline,signal);}
  catch(...){close();throw;}
#else
  std::size_t sent = 0;
  try {while (sent < line.size()) {
    ready(input, POLLOUT, deadline,signal);
#if defined(__APPLE__)
    const auto written = ::send(input, line.data()+sent, line.size()-sent, 0);
#else
    const auto written = ::send(input, line.data()+sent, line.size()-sent, MSG_NOSIGNAL);
#endif
    if (written < 0 && (errno == EINTR || errno == EAGAIN)) continue;
    if (written <= 0) throw std::runtime_error("Cannot write to Scrapling");
    sent += written;
  }}catch(...){if(sent>0 && sent<line.size())close();throw;}
#endif
}
Json Scrapling::receive(std::chrono::steady_clock::time_point deadline,std::stop_token signal) {
#ifdef _WIN32
  if(!indigenousProcess)throw std::runtime_error("Scrapling process disconnected");
  while(true){auto line=indigenousProcess->readLine(deadline,signal);
    if(line.empty()||line=="\r")continue;
    return value::parse(line);}
#else
  while (true) {
    checkConnectorAbort(signal);
    const auto newline = buffered.find('\n');
    if (newline != std::string::npos) {
      auto line = buffered.substr(0,newline); buffered.erase(0,newline+1);
      if (line.empty() || line == "\r") continue;
      return value::parse(line);
    }
    ready(output, POLLIN, deadline,signal);
    char bytes[16384];
    const auto count = ::read(output, bytes, sizeof(bytes));
    if (count < 0 && (errno == EINTR || errno == EAGAIN)) continue;
    if (count <= 0) throw std::runtime_error("Scrapling output ended before the response");
    buffered.append(bytes,count);
  }
#endif
}
Json Scrapling::request(const std::string& method, const Json& params,std::stop_token signal) {
  checkConnectorAbort(signal);
  const auto deadline = std::chrono::steady_clock::now()+timeout;
  const auto id = ++nextId;
  send({{"jsonrpc","2.0"},{"id",id},{"method",method},{"params",params}},deadline,signal);
  try {while (true) {
    const auto message = receive(deadline,signal);
    if (message.contains("method")) {
      if (message.contains("id")) {
        if(message.at("method")=="ping")send({{"jsonrpc","2.0"},{"id",message.at("id")},{"result",Json::object()}},deadline,signal);
        else send({{"jsonrpc","2.0"},{"id",message.at("id")},
          {"error",{{"code",-32601},{"message","Client method unavailable"}}}},deadline,signal);
      }
      continue;
    }
    if (value::property(message,"id") != id) continue;
    if (message.contains("error")) throw std::runtime_error("Scrapling MCP error: "+value::stringify(message.at("error")));
    return message.at("result");
  }}catch(const ConnectorAbortError&){
    // Late replies to this ID are discarded by the next request. The normal
    // cancellation path keeps the process and browser flights alive.
    if(
#ifdef _WIN32
        indigenousProcess
#else
        input>=0
#endif
        && method!="initialize")try {
      send({{"jsonrpc","2.0"},{"method","notifications/cancelled"},
        {"params",{{"requestId",id},{"reason","Operation aborted"}}}},
        std::min(deadline,std::chrono::steady_clock::now()+std::chrono::milliseconds(100)));
    }catch(...){}
    throw;
  }
}
std::unique_lock<std::timed_mutex> Scrapling::acquire(std::stop_token signal) {
  checkConnectorAbort(signal);
  std::unique_lock lock(mutex,std::defer_lock);
  while(!lock.try_lock_for(std::chrono::milliseconds(50)))checkConnectorAbort(signal);
  checkConnectorAbort(signal);
  return lock;
}
Json Scrapling::listGadgets(std::stop_token signal) { auto lock=acquire(signal); return request("tools/list",Json::object(),signal); }
Json Scrapling::callGadget(const std::string& name, const Json& arguments,std::stop_token signal) {
  auto lock=acquire(signal);
  return request("tools/call",{{"name",name},{"arguments",arguments}},signal);
}
Json Scrapling::fetch(const std::string& url,std::stop_token signal) { return callGadget("make_request",{{"url",url},{"extraction_type","markdown"}},signal); }
} // namespace batbox
