#include "shell/SnapshotRun.hpp"
#include "shell/WindowsPaths.hpp"
#include "runtime/IndigenousPath.hpp"
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace batbox::shell_snapshot {
namespace {
void require(bool ready,const char* name){
  if(!ready)throw std::invalid_argument(std::string("Snapshot service required: ")+name);
}
std::wstring wide(const std::string& value){return runtime::pathFromUtf8(value).native();}
std::wstring quote(const std::wstring& value){
  std::wstring out=L"\"";std::size_t slashes=0;
  for(wchar_t c:value){
    if(c==L'\\'){++slashes;continue;}
    if(c==L'"'){out.append(slashes*2+1,L'\\');out+=c;slashes=0;continue;}
    out.append(slashes,L'\\');slashes=0;out+=c;
  }
  out.append(slashes*2,L'\\');return out+L'"';
}
struct Execution {int code=1;std::string out,err;bool timedOut=false,tooLarge=false;};
struct Pipe {HANDLE read=nullptr,write=nullptr;
  ~Pipe(){if(read)::CloseHandle(read);if(write)::CloseHandle(write);}
};
void makePipe(Pipe& pipe){
  SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};
  if(!::CreatePipe(&pipe.read,&pipe.write,&security,0)||
     !::SetHandleInformation(pipe.read,HANDLE_FLAG_INHERIT,0))
    throw std::runtime_error("snapshot output pipe failed");
}
void drain(HANDLE pipe,std::string& output,bool& tooLarge){
  DWORD available=0;
  if(!::PeekNamedPipe(pipe,nullptr,0,nullptr,&available,nullptr))return;
  while(available>0){
    char buffer[4096];DWORD count=0;
    if(!::ReadFile(pipe,buffer,std::min<DWORD>(available,sizeof(buffer)),&count,nullptr)||count==0)return;
    if(output.size()+count>1024*1024){tooLarge=true;return;}
    output.append(buffer,count);available-=count;
  }
}
std::vector<wchar_t> environmentBlock(const std::map<std::string,std::string>& environment){
  std::vector<wchar_t> block;
  for(const auto& [key,value]:environment){
    const auto entry=wide(key+"="+value);
    block.insert(block.end(),entry.begin(),entry.end());block.push_back(L'\0');
  }
  if(block.empty())block.push_back(L'\0');
  block.push_back(L'\0');return block;
}
Execution runScript(const std::string& shellPath,const std::string& script,
    const std::map<std::string,std::string>& environment){
  Pipe out,err;makePipe(out);makePipe(err);
  auto env=environmentBlock(environment);
  const auto shell=wide(shellPath);
  auto command=quote(shell)+L" -c -l "+quote(wide(script));
  std::vector<wchar_t> args(command.begin(),command.end());args.push_back(L'\0');
  STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES;
  SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};
  HANDLE input=::CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,
    &security,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(input==INVALID_HANDLE_VALUE)throw std::runtime_error("snapshot NUL input failed");
  startup.hStdInput=input;
  startup.hStdOutput=out.write;startup.hStdError=err.write;
  PROCESS_INFORMATION process{};
  const bool started=::CreateProcessW(shell.c_str(),args.data(),nullptr,nullptr,TRUE,
      CREATE_UNICODE_ENVIRONMENT|CREATE_NEW_PROCESS_GROUP|CREATE_SUSPENDED|CREATE_NO_WINDOW,
      env.data(),nullptr,&startup,&process);
  const auto spawnError=::GetLastError();::CloseHandle(input);
  if(!started)
    throw std::runtime_error("snapshot spawn failed: Windows error "+std::to_string(spawnError));
  ::CloseHandle(std::exchange(out.write,nullptr));
  ::CloseHandle(std::exchange(err.write,nullptr));
  HANDLE group=::CreateJobObjectW(nullptr,nullptr);
  if(group){
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!::SetInformationJobObject(group,JobObjectExtendedLimitInformation,&limits,sizeof(limits))||
       !::AssignProcessToJobObject(group,process.hProcess)){
      ::CloseHandle(group);group=nullptr;
    }
  }
  if(::ResumeThread(process.hThread)==static_cast<DWORD>(-1)){
    ::TerminateProcess(process.hProcess,137);
    ::CloseHandle(process.hThread);::CloseHandle(process.hProcess);
    if(group)::CloseHandle(group);
    throw std::runtime_error("snapshot resume failed");
  }
  ::CloseHandle(process.hThread);
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
  Execution result;bool exited=false,killed=false;
  for(;;){
    drain(out.read,result.out,result.tooLarge);drain(err.read,result.err,result.tooLarge);
    const auto wait=::WaitForSingleObject(process.hProcess,20);
    if(wait==WAIT_OBJECT_0||wait==WAIT_FAILED)exited=true;
    if(!killed&&(result.tooLarge||std::chrono::steady_clock::now()>=deadline)){
      result.timedOut=std::chrono::steady_clock::now()>=deadline;killed=true;
      if(group)::TerminateJobObject(group,137);else ::TerminateProcess(process.hProcess,137);
    }
    if(exited){
      drain(out.read,result.out,result.tooLarge);drain(err.read,result.err,result.tooLarge);
      break;
    }
  }
  DWORD code=1;::GetExitCodeProcess(process.hProcess,&code);
  result.code=static_cast<int>(code);
  ::CloseHandle(process.hProcess);if(group)::CloseHandle(group);
  return result;
}
std::string shellType(const std::string& path){
  return path.find("zsh")!=std::string::npos?"zsh":
    path.find("bash")!=std::string::npos?"bash":"sh";
}
}
std::optional<std::string> createAndSaveSnapshot(const std::string& shellPath,
    const RunServices& services){
  require(bool(services.configHome),"configHome");
  require(bool(services.homeDirectory),"homeDirectory");
  require(bool(services.nowMillis),"nowMillis");
  require(bool(services.randomId),"randomId");
  require(bool(services.subprocessEnvironment),"subprocessEnvironment");
  require(bool(services.registerCleanup),"registerCleanup");
  require(bool(services.diagnostic),"diagnostic");
  try{
    const auto home=services.homeDirectory();
    const auto configFile=runtime::pathFromUtf8(home)/".bashrc";
    const bool configExists=std::filesystem::exists(configFile);
    const auto dir=runtime::pathFromUtf8(services.configHome())/"shell-snapshots";
    const auto path=dir/("snapshot-"+shellType(shellPath)+"-"+
      std::to_string(services.nowMillis())+"-"+services.randomId()+".sh");
    std::filesystem::create_directories(dir);
    const auto indigenousPath=runtime::pathToUtf8(path);
    const auto script=getSnapshotScript(shellPath,shell::windowsPathToPosixPath(indigenousPath),
      configExists,shell::windowsPathToPosixPath(home),services.content);
    auto env=services.dontInheritEnvironment?std::map<std::string,std::string>{}:
      services.subprocessEnvironment();
    env["SHELL"]=shellPath;env["GIT_EDITOR"]="true";env["BATBOX"]="1";
    const auto result=runScript(shellPath,script,env);
    if(result.code!=0||result.timedOut||result.tooLarge){
      services.diagnostic("Shell snapshot creation failed: exit "+std::to_string(result.code)+
        (result.timedOut?" (timeout)":"")+(result.tooLarge?" (output limit)":"")+
        (result.err.empty()?"":"; stderr: "+result.err));return std::nullopt;
    }
    std::error_code error;(void)std::filesystem::file_size(path,error);
    if(error){services.diagnostic("Shell snapshot file missing after creation: "+indigenousPath);
      return std::nullopt;}
    services.registerCleanup([indigenousPath]{std::error_code ignored;
      std::filesystem::remove(runtime::pathFromUtf8(indigenousPath),ignored);});
    return indigenousPath;
  }catch(const std::exception& error){
    services.diagnostic(std::string("Unexpected shell snapshot error: ")+error.what());
    return std::nullopt;
  }
}
} // namespace batbox::shell_snapshot
