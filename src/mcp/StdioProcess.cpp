#include "runtime/AsyncLaunch.hpp"
#include "mcp/StdioClient.hpp"
#include "process/IndigenousJsonlProcess.hpp"
#include "runtime/JsonValue.hpp"
#include <cerrno>
#include <algorithm>
#include <cctype>
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
#ifdef _WIN32
#include <uv.h>
#endif

#ifndef _WIN32
extern char** environ;
#endif
namespace batbox::mcp {
namespace {
constexpr std::size_t maxStderrBytes=64*1024;
constexpr std::size_t maxInstructions=2048;
bool sensitiveName(std::string name){
  std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){
    return static_cast<char>(std::toupper(c));});
  for(const char* word:{"TOKEN","SECRET","PASSWORD","PASSWD","API_KEY",
      "PRIVATE_KEY","AUTH","CREDENTIAL","BEARER"})
    if(name.find(word)!=std::string::npos)return true;
  return false;
}
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
std::string printableDiagnostic(const std::string& source,
    const std::vector<std::string>& secretValues){
  std::string text; text.reserve(source.size());
  for(unsigned char c:source){
    if(c=='\n'||c=='\t')text.push_back(static_cast<char>(c));
    else if(c=='\r')text.push_back('\n');
    else if(c>=0x20&&c!=0x7f)text.push_back(static_cast<char>(c));
  }
  for(const auto& value:secretValues){
    if(value.size()<4)continue;
    std::size_t at=0;
    while((at=text.find(value,at))!=std::string::npos){
      text.replace(at,value.size(),"[redacted]");at+=10;
    }
  }
  std::string safe;
  std::size_t start=0;
  while(start<text.size()){
    const auto end=text.find('\n',start);
    const auto length=end==std::string::npos?text.size()-start:end-start;
    const auto line=text.substr(start,length);
    safe+=environmentAssignment(line)?"[environment assignment redacted]":line;
    if(end==std::string::npos)break;
    safe+='\n';start=end+1;
  }
  return value::decodeUtf8(safe);
}
#ifndef _WIN32
#if !defined(__linux__)
bool closeOnExec(int fd){
  const auto flags=::fcntl(fd,F_GETFD);
  return flags>=0&&::fcntl(fd,F_SETFD,flags|FD_CLOEXEC)>=0;
}
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
#endif
}
StdioClient::StdioClient(const std::vector<std::string>& command,const std::string& serverName,
  std::chrono::milliseconds timeout,
  const std::map<std::string,std::string>& environmentOverrides,
  std::optional<std::chrono::milliseconds> initializationTimeout,
  std::optional<std::chrono::milliseconds> ordinaryRequestTimeout,ClientHandlers handlers,
  std::stop_token initializeSignal):
  serverName_(serverName),timeout_(timeout),initializationTimeout_(initializationTimeout),
  ordinaryRequestTimeout_(ordinaryRequestTimeout),handlers_(std::move(handlers)){
  if(command.empty())throw std::invalid_argument("Set an MCP server executable");
  for(const auto& [name,value]:environmentOverrides)
    if(name.empty()||name.find('=')!=std::string::npos||
       name.find('\0')!=std::string::npos||value.find('\0')!=std::string::npos)
      throw std::invalid_argument("Invalid MCP environment override");
#ifdef _WIN32
  uv_env_item_t* inherited=nullptr;int count=0;
  if(uv_os_environ(&inherited,&count)==0){
    for(int index=0;index<count;++index)
      if(sensitiveName(inherited[index].name)&&inherited[index].value&&
         *inherited[index].value)
        sensitiveEnvironmentValues_.push_back(inherited[index].value);
    uv_os_free_environ(inherited,count);
  }
  for(const auto& [name,value]:environmentOverrides)
    if(sensitiveName(name)&&!value.empty())sensitiveEnvironmentValues_.push_back(value);
  indigenousProcess_=std::make_unique<process::IndigenousJsonlProcess>(command,environmentOverrides);
  connected_=true;
  reader_=::batbox::runtime::contextThread([this]{readLoop();});
#else
  int toChild[2],fromChild[2],errChild[2];
  if(!indigenousSocketPair(toChild))
    throw std::system_error(errno,std::generic_category(),"MCP input channel");
  if(!indigenousPipe(fromChild)){::close(toChild[0]);::close(toChild[1]);
    throw std::system_error(errno,std::generic_category(),"MCP output channel");}
  if(!indigenousPipe(errChild)){
    ::close(toChild[0]);::close(toChild[1]);
    ::close(fromChild[0]);::close(fromChild[1]);
    throw std::system_error(errno,std::generic_category(),"MCP diagnostic channel");
  }
  posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_adddup2(&actions,toChild[0],STDIN_FILENO);
  posix_spawn_file_actions_adddup2(&actions,fromChild[1],STDOUT_FILENO);
  posix_spawn_file_actions_adddup2(&actions,errChild[1],STDERR_FILENO);
  for(int fd:{toChild[0],toChild[1],fromChild[0],fromChild[1],errChild[0],errChild[1]})
    posix_spawn_file_actions_addclose(&actions,fd);
  posix_spawnattr_t attributes;posix_spawnattr_init(&attributes);
  posix_spawnattr_setflags(&attributes,POSIX_SPAWN_SETPGROUP);
  posix_spawnattr_setpgroup(&attributes,0);
  std::vector<char*> argv;
  for(const auto& arg:command)argv.push_back(const_cast<char*>(arg.c_str()));
  argv.push_back(nullptr);
  std::map<std::string,std::string> environment;
  for(char** entry=environ;entry&&*entry;++entry){
    const std::string value=*entry;
    const auto equals=value.find('=');
    if(equals!=std::string::npos)environment[value.substr(0,equals)]=value.substr(equals+1);
  }
  for(const auto& [name,value]:environmentOverrides)environment[name]=value;
  for(const auto& [name,value]:environment)
    if(sensitiveName(name)&&!value.empty())sensitiveEnvironmentValues_.push_back(value);
  std::vector<std::string> environmentText;
  environmentText.reserve(environment.size());
  for(const auto& [name,value]:environment)environmentText.push_back(name+"="+value);
  std::vector<char*> environmentPointers;
  environmentPointers.reserve(environmentText.size()+1);
  for(auto& value:environmentText)environmentPointers.push_back(value.data());
  environmentPointers.push_back(nullptr);
  const int error=posix_spawnp(&child_,argv[0],&actions,&attributes,
    argv.data(),environmentPointers.data());
  posix_spawn_file_actions_destroy(&actions);posix_spawnattr_destroy(&attributes);
  ::close(toChild[0]);::close(fromChild[1]);::close(errChild[1]);
  input_=toChild[1];output_=fromChild[0];stderr_=errChild[0];
  if(error){child_=-1;close();throw std::runtime_error(
    std::string("Cannot start MCP server: ")+std::strerror(error));}
  ::fcntl(input_,F_SETFL,::fcntl(input_,F_GETFL)|O_NONBLOCK);
#if defined(__APPLE__)
  int noSigpipe=1;::setsockopt(input_,SOL_SOCKET,SO_NOSIGPIPE,&noSigpipe,sizeof(noSigpipe));
#endif
  connected_=true;
  stderrReader_=::batbox::runtime::contextThread([this]{stderrLoop();});
  reader_=::batbox::runtime::contextThread([this]{readLoop();});
#endif
  try{
    Json clientCapabilities={{"roots",Json::object()}};
    if(handlers_.requestAsync)
      clientCapabilities["elicitation"]={{"form",Json::object()},
        {"url",Json::object()}};
    const auto initialized=request("initialize",{{"protocolVersion","2025-11-25"},
      {"capabilities",std::move(clientCapabilities)},
      {"clientInfo",{{"name","batbox"},{"title","Batbox"},{"version","0.0.1"}}}},initializeSignal);
    if(!initialized.contains("protocolVersion"))
      throw std::runtime_error("MCP initialization returned no protocol version");
    gadgetsCapability_=value::truthy(value::property(value::property(initialized,"capabilities"),"tools"));
    capabilities_=value::property(initialized,"capabilities");serverInfo_=value::property(initialized,"serverInfo");
    const auto& rawInstructions=value::property(initialized,"instructions");
    if(rawInstructions.is_string()){
      instructions_=rawInstructions.get<std::string>();
      if(value::stringLength(instructions_)>maxInstructions)
        instructions_=value::slice(instructions_,0,maxInstructions)+"… [truncated]";
    }
    send({{"jsonrpc","2.0"},{"method","notifications/initialized"}},
      std::chrono::steady_clock::now()+timeout_);
  }catch(const std::exception& failure){
    const std::string reason=failure.what();
    close();
    const auto diagnostic=stderrDiagnostic();
    if(!diagnostic.empty())
      throw std::runtime_error(reason+"; server stderr: "+diagnostic);
    throw std::runtime_error(reason);
  }catch(...){close();throw;}
}
StdioClient::~StdioClient(){close();}
std::string StdioClient::stderrDiagnostic() const{
  std::string tail;
  bool truncated;
#ifdef _WIN32
  if(indigenousProcess_){tail=indigenousProcess_->stderrTail();truncated=indigenousProcess_->stderrTruncated();}
  else truncated=false;
#else
  {std::lock_guard lock(stderrMutex_);
    tail=stderrTail_;truncated=stderrTruncated_;}
#endif
  auto safe=printableDiagnostic(tail,sensitiveEnvironmentValues_);
  if(truncated)safe="[earlier stderr truncated]\n"+safe;
  return safe;
}
void StdioClient::stderrLoop(){
#ifndef _WIN32
  for(;;){
    pollfd item{stderr_,POLLIN,0};
    const auto ready=::poll(&item,1,100);
    if(ready<0&&errno==EINTR)continue;
    if(ready<=0){if(stderrStop_||ready<0)break;continue;}
    char bytes[4096];
    const auto count=::read(stderr_,bytes,sizeof(bytes));
    if(count<0&&errno==EINTR)continue;
    if(count<=0)break;
    std::lock_guard lock(stderrMutex_);
    stderrTail_.append(bytes,static_cast<std::size_t>(count));
    if(stderrTail_.size()>maxStderrBytes){
      stderrTail_.erase(0,stderrTail_.size()-maxStderrBytes);
      stderrTruncated_=true;
    }
  }
#endif
}
void StdioClient::ensureConnected() const{
  if(!connected_)throw std::runtime_error("MCP server disconnected");
}
void StdioClient::close() noexcept{
  connected_=false;
  requestLifetime_.request_stop();
  failAll(std::make_exception_ptr(std::runtime_error("MCP server disconnected")));
#ifdef _WIN32
  if(indigenousProcess_)indigenousProcess_->close();
  if(reader_.joinable())reader_.join();
#else
  if(input_>=0){::shutdown(input_,SHUT_RDWR);::close(input_);input_=-1;}
  if(child_>0){::kill(-child_,SIGTERM);}
  if(child_>0){
    int status=0;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(500);
    while(std::chrono::steady_clock::now()<deadline){
      const auto done=::waitpid(child_,&status,WNOHANG);
      if(done==child_||(done<0&&errno==ECHILD)){child_=-1;break;}
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if(child_>0){::kill(-child_,SIGKILL);
      while(::waitpid(child_,&status,0)<0&&errno==EINTR){}child_=-1;}
  }
  if(reader_.joinable())reader_.join();
  if(output_>=0){::close(output_);output_=-1;}
  stderrStop_=true;
  if(stderrReader_.joinable())stderrReader_.join();
  if(stderr_>=0){::close(stderr_);stderr_=-1;}
#endif
}
std::shared_ptr<Connection> makeStdioConnection(const std::shared_ptr<StdioClient>& client,
  const std::string& serverName,bool skipPrefix){
  if(!client)throw std::invalid_argument("Missing MCP stdio client");
  auto connection=std::make_shared<Connection>();
  connection->name=serverName;connection->connected=true;
  connection->hasGadgetCapability=client->hasGadgetCapability();
  connection->instructions=client->instructions();
  connection->capabilities=client->capabilities();connection->serverInfo=client->serverInfo();
  connection->skipPrefix=skipPrefix;
  connection->listGadgets=[client](std::stop_token signal){return client->listGadgets(signal);};
  connection->request=[client](const std::string& method,const Json& params,std::stop_token signal){
    return client->invoke(method,params,signal);};
  connection->callGadget=[client](const std::string& name,const Json& args,
    std::stop_token signal,const std::function<void(const Json&)>& onProgress){
      return client->callGadget(name,args,signal,onProgress);};
  connection->ensureConnected=[client](){client->ensureConnected();};
  return connection;
}
} // namespace batbox::mcp
