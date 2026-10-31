#include "runtime/AsyncLaunch.hpp"
#include "runtime/IndigenousPath.hpp"
#include "shell/ManagedShellInternal.hpp"
#include "shell/BashProvider.hpp"
#include "shell/ShellCwdCompletion.hpp"
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cwchar>
#include <filesystem>
#include <map>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace batbox::shell {
namespace {
struct Handles {
  HANDLE process=nullptr,group=nullptr;
  ~Handles(){if(process)::CloseHandle(process);if(group)::CloseHandle(group);}
};
std::mutex processMutex;
std::map<ShellProcessId,std::shared_ptr<Handles>> processes;
std::wstring wide(const std::string& value){return runtime::pathFromUtf8(value).native();}
std::wstring quote(const std::wstring& value){
  std::wstring result=L"\"";std::size_t slashes=0;
  for(wchar_t c:value){
    if(c==L'\\'){++slashes;continue;}
    if(c==L'"'){result.append(slashes*2+1,L'\\');result+=c;slashes=0;continue;}
    result.append(slashes,L'\\');slashes=0;result+=c;
  }
  result.append(slashes*2,L'\\');return result+L'"';
}
struct CaseLess {bool operator()(const std::wstring& a,const std::wstring& b)const{
  return _wcsicmp(a.c_str(),b.c_str())<0;
}};
std::vector<wchar_t> childEnvironment(const FlightConfig& config,const RunOptions& options){
  std::map<std::wstring,std::wstring,CaseLess> values;
  auto inherited=::GetEnvironmentStringsW();
  if(!inherited)throw std::runtime_error("read shell environment failed");
  for(const wchar_t* entry=inherited;*entry;entry+=wcslen(entry)+1){
    const wchar_t* equals=wcschr(entry+(*entry==L'='),L'=');
    if(equals)values[std::wstring(entry,equals)]=equals+1;
  }
  ::FreeEnvironmentStringsW(inherited);
  for(const auto& [key,value]:config.environmentOverrides)values[wide(key)]=wide(value);
  values[L"SHELL"]=wide(config.shellPath);values[L"GIT_EDITOR"]=L"true";values[L"BATBOX"]=L"1";
  if(config.provider)for(const auto& [key,value]:config.provider->getEnvironmentOverrides(options.command))
    values[wide(key)]=wide(value);
  std::vector<wchar_t> block;
  for(const auto& [key,value]:values){
    block.insert(block.end(),key.begin(),key.end());block.push_back(L'=');
    block.insert(block.end(),value.begin(),value.end());block.push_back(L'\0');
  }
  block.push_back(L'\0');return block;
}
std::wstring commandLine(const std::vector<std::string>& args){
  std::wstring line;
  for(const auto& arg:args){if(!line.empty())line+=L' ';line+=quote(wide(arg));}
  return line;
}
std::runtime_error winError(const std::string& text){
  return std::runtime_error(text+": Windows error "+std::to_string(::GetLastError()));
}
bool safeMissionId(const std::string& id){
  if(id.empty())return false;
  for(char c:id)if(!std::isalnum(static_cast<unsigned char>(c))&&c!='-'&&c!='_')return false;
  return true;
}
}
void terminateGroup(ShellProcessId pid){
  std::lock_guard lock(processMutex);const auto it=processes.find(pid);
  if(it==processes.end())return;
  const auto& handles=*it->second;
  if(handles.group)::TerminateJobObject(handles.group,137);
  else ::TerminateProcess(handles.process,137);
}
std::shared_ptr<ManagedShell::Job> ManagedShell::spawn(const RunOptions& options,
    const std::string& missionId,const std::string& cwd){
  if(!safeMissionId(missionId))throw std::invalid_argument("invalid shell task ID");
  std::filesystem::create_directories(runtime::pathFromUtf8(config_.outputDir));
  auto job=std::make_shared<Job>();job->missionId=missionId;job->command=options.command;
  job->description=options.description.empty()?options.command:options.description;
  job->outputPath=outputPath(missionId);job->cwdPath=config_.outputDir+"/"+missionId+"-cwd";
  job->startingCwd=cwd;job->started=std::chrono::steady_clock::now();
  std::string script;
  if(config_.provider){
    if(!options.preparedCommandString||!options.preparedCwdFilePath)
      throw std::invalid_argument("provider command must be prepared before spawn");
    script=*options.preparedCommandString;job->cwdPath=*options.preparedCwdFilePath;
  }else{
    script="eval "+quotePosix(options.command)+"; batbox_status=$?; "
      "if [ \"$batbox_status\" -eq 0 ]; then pwd -P >| "+
      quotePosix(job->cwdPath)+"; fi; exit \"$batbox_status\"";
  }
  if(options.useSandbox){
    if(!options.sandboxTmpDir||!options.sandboxWrap||!options.logDebug)
      throw std::invalid_argument("sandbox wrapping, temp directory, and diagnostic services required");
    script=options.sandboxWrap(script);
    std::error_code error;
    std::filesystem::create_directory(runtime::pathFromUtf8(*options.sandboxTmpDir),error);
    if(error)options.logDebug("Failed to create "+*options.sandboxTmpDir+" directory: "+error.message());
  }
  std::vector<std::string> args{config_.shellPath};
  if(config_.provider)for(const auto& arg:config_.provider->getSpawnArgs(script))args.push_back(arg);
  else {args.push_back("-c");args.push_back("-l");args.push_back(script);}
  auto environment=childEnvironment(config_,options);
  auto line=commandLine(args);std::vector<wchar_t> writable(line.begin(),line.end());writable.push_back(L'\0');
  const auto indigenousOutput=runtime::pathFromUtf8(job->outputPath);
  SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};
  // Windows output needs write access: FILE_APPEND_DATA alone can silently
  // discard writes from a child that expects FILE_WRITE_DATA.
  HANDLE output=::CreateFileW(indigenousOutput.c_str(),GENERIC_WRITE,
    FILE_SHARE_READ|FILE_SHARE_WRITE,&security,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(output==INVALID_HANDLE_VALUE)throw winError("open shell output failed");
  HANDLE input=::CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,
    &security,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(input==INVALID_HANDLE_VALUE){::CloseHandle(output);throw winError("open NUL failed");}
  STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES;
  startup.hStdInput=input;startup.hStdOutput=output;startup.hStdError=output;
  PROCESS_INFORMATION info{};const auto executable=wide(config_.shellPath),working=wide(cwd);
  const bool started=::CreateProcessW(executable.c_str(),writable.data(),nullptr,nullptr,
    TRUE,CREATE_UNICODE_ENVIRONMENT|CREATE_NEW_PROCESS_GROUP|CREATE_SUSPENDED|CREATE_NO_WINDOW,
    environment.data(),working.c_str(),&startup,&info);
  const auto spawnError=::GetLastError();
  ::CloseHandle(input);::CloseHandle(output);
  if(!started)throw std::runtime_error("spawn shell failed: Windows error "+std::to_string(spawnError));
  auto handles=std::make_shared<Handles>();handles->process=info.hProcess;
  handles->group=::CreateJobObjectW(nullptr,nullptr);
  if(handles->group){
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!::SetInformationJobObject(handles->group,JobObjectExtendedLimitInformation,
          &limits,sizeof(limits))||!::AssignProcessToJobObject(handles->group,info.hProcess)){
      ::CloseHandle(handles->group);handles->group=nullptr;
    }
  }
  job->pid=info.dwProcessId;
  {std::lock_guard lock(processMutex);processes[job->pid]=handles;}
  if(::ResumeThread(info.hThread)==static_cast<DWORD>(-1)){
    terminateGroup(job->pid);::CloseHandle(info.hThread);
    {std::lock_guard lock(processMutex);processes.erase(job->pid);}
    throw winError("resume shell failed");
  }
  ::CloseHandle(info.hThread);
  auto completion=config_.onBackgroundComplete;
  auto completionError=config_.onBackgroundError;
  auto cwdCompletion=config_.cwdCompletion;
  const bool sandbox=options.useSandbox,preventCwdChanges=options.preventCwdChanges;
  const auto maxInline=config_.maxInlineOutputBytes;
  ::batbox::runtime::contextThread([job,handles,completion,completionError,cwdCompletion,
      sandbox,preventCwdChanges,maxInline]{
    const auto waited=::WaitForSingleObject(handles->process,INFINITE);
    DWORD status=1;if(waited==WAIT_OBJECT_0)::GetExitCodeProcess(handles->process,&status);
    {std::lock_guard lock(processMutex);processes.erase(job->pid);}
    ExecResult notification;
    {
      std::lock_guard lock(job->mutex);job->completed=true;
      job->code=job->timedOut?143:job->killed?137:
        waited!=WAIT_OBJECT_0?1:static_cast<int>(status);
      notification.code=job->code;
      notification.backgroundMissionId=job->background?std::optional(job->missionId):std::nullopt;
      notification.interrupted=job->code==137;
    }
    if(notification.backgroundMissionId){
      if(cwdCompletion){
        try{completeShellCwd(&notification,preventCwdChanges,sandbox,
          job->cwdPath,job->startingCwd,*cwdCompletion);}
        catch(...){try{cwdCompletion->onFailure(std::current_exception());}catch(...){}}
      }else{std::error_code error;std::filesystem::remove(runtime::pathFromUtf8(job->cwdPath),error);}
    }
    {std::lock_guard lock(job->mutex);job->postExitReady=true;}
    job->cv.notify_all();
    if(completion&&notification.backgroundMissionId){
      try{
        std::size_t totalBytes=0;
        notification.stdoutText=readOutputPrefix(job->outputPath,maxInline,totalBytes);
        if(totalBytes>maxInline){notification.outputFilePath=job->outputPath;
          notification.outputFileSize=totalBytes;notification.outputMissionId=job->missionId;}
      }catch(const std::exception& error){
        notification.stdoutText="<bash output unavailable: output file "+job->outputPath+
          " could not be read. "+error.what()+">";
      }
      try{completion(job->missionId,notification);}
      catch(...){const auto failure=std::current_exception();
        {std::lock_guard lock(job->mutex);job->completionFailure=failure;}
        if(completionError)try{completionError(job->missionId,failure);}catch(...){}}
    }
    {std::lock_guard lock(job->mutex);job->callbackDone=true;}
    job->cv.notify_all();
  }).detach();
  return job;
}
} // namespace batbox::shell
