#include "application_editor/IndigenousProcess.hpp"
#include "runtime/IndigenousUnicode.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace batbox::application_editor::indigenous {
namespace {
std::string trim(std::string value){
  const auto first=value.find_first_not_of(" \t\r\n");
  if(first==std::string::npos)return {};
  const auto last=value.find_last_not_of(" \t\r\n");
  return value.substr(first,last-first+1);
}
std::vector<std::string> splitCommand(const std::string& value){
  std::vector<std::string> parts;std::string current;
  char quote=0;bool started=false;
  for(std::size_t index=0;index<value.size();++index){
    const char c=value[index];
    if(c=='\\'&&quote!='\''&&index+1<value.size()){
      const char next=value[index+1];
      if(next=='\\'||next=='\"'||(!quote&&
          (next=='\''||std::isspace(static_cast<unsigned char>(next))))){
        current+=next;++index;started=true;continue;
      }
    }
    if(quote){if(c==quote)quote=0;else current+=c;continue;}
    if(c=='\''||c=='\"'){quote=c;started=true;continue;}
    if(std::isspace(static_cast<unsigned char>(c))){
      if(started){parts.push_back(current);current.clear();started=false;}
    }else{current+=c;started=true;}
  }
  if(quote)throw std::invalid_argument("Unclosed quote in external editor command");
  if(started)parts.push_back(current);
  return parts;
}
std::string lower(std::string value){
  std::transform(value.begin(),value.end(),value.begin(),
    [](unsigned char c){return static_cast<char>(std::tolower(c));});
  return value;
}
std::string basename(const std::string& value){
  return std::filesystem::path(value).filename().string();
}
std::string display(const std::string& command){
  const auto name=lower(basename(command));
  if(name=="code")return "VS Code";
  if(name=="cursor")return "Cursor";
  if(name=="windsurf")return "Windsurf";
  if(name=="vi"||name=="vim")return "Vim";
  if(name=="nano")return "nano";
  if(name=="notepad"||name=="notepad.exe")return "Notepad";
  if(name=="emacs")return "Emacs";
  if(name=="subl")return "Sublime Text";
  if(name=="atom")return "Atom";
  if(name.empty())return "IDE";
  auto result=name;result.front()=static_cast<char>(std::toupper(
    static_cast<unsigned char>(result.front())));
  return result;
}
bool isGui(const std::string& command){
  const auto name=lower(basename(command));
  for(const auto* family:{"code","cursor","windsurf","codium","subl",
      "atom","gedit","notepad++","notepad"})
    if(name.find(family)!=std::string::npos)return true;
  return false;
}
#ifndef _WIN32
bool available(const std::string& command){
  if(command.empty())return false;
  if(command.find('/')!=std::string::npos)return ::access(command.c_str(),X_OK)==0;
  const char* raw=std::getenv("PATH");
  const std::string path=raw?raw:"";
  std::size_t start=0;
  while(start<=path.size()){
    const auto end=path.find(':',start);
    const auto dir=path.substr(start,end==std::string::npos?end:end-start);
    const auto candidate=(std::filesystem::path(dir.empty()?".":dir)/command).string();
    if(::access(candidate.c_str(),X_OK)==0)return true;
    if(end==std::string::npos)break;
    start=end+1;
  }
  return false;
}
ProcessResult spawn(const std::vector<std::string>& args,bool detached){
  if(args.empty())return {0,"Editor command is empty"};
  std::vector<char*> argv;argv.reserve(args.size()+1);
  for(const auto& arg:args)argv.push_back(const_cast<char*>(arg.c_str()));
  argv.push_back(nullptr);
  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_t* fileActions=nullptr;
  posix_spawnattr_t attributes;
  posix_spawnattr_t* spawnAttributes=nullptr;
  if(detached){
    if(posix_spawn_file_actions_init(&actions)!=0)
      return {0,"Could not prepare detached editor"};
    if(posix_spawnattr_init(&attributes)!=0){
      posix_spawn_file_actions_destroy(&actions);
      return {0,"Could not prepare detached editor group"};
    }
    posix_spawnattr_setflags(&attributes,POSIX_SPAWN_SETPGROUP);
    posix_spawnattr_setpgroup(&attributes,0);
    posix_spawn_file_actions_addopen(&actions,0,"/dev/null",O_RDONLY,0);
    posix_spawn_file_actions_addopen(&actions,1,"/dev/null",O_WRONLY,0);
    posix_spawn_file_actions_addopen(&actions,2,"/dev/null",O_WRONLY,0);
    fileActions=&actions;
    spawnAttributes=&attributes;
  }
  pid_t pid=-1;
  const int code=posix_spawnp(&pid,args.front().c_str(),fileActions,spawnAttributes,
    argv.data(),environ);
  if(fileActions)posix_spawn_file_actions_destroy(fileActions);
  if(spawnAttributes)posix_spawnattr_destroy(spawnAttributes);
  if(code!=0)return {0,std::string("Could not launch editor: ")+std::strerror(code)};
  if(detached){
    std::thread([pid]{int status=0;while(::waitpid(pid,&status,0)<0&&errno==EINTR){}
    }).detach();
    return {};
  }
  int status=0;
  while(::waitpid(pid,&status,0)<0){
    if(errno==EINTR)continue;
    return {0,std::string("Could not wait for editor: ")+std::strerror(errno)};
  }
  if(WIFEXITED(status))return {WEXITSTATUS(status),{}};
  if(WIFSIGNALED(status))return {128+WTERMSIG(status),{}};
  return {0,"Editor did not report an exit status"};
}
#else
std::wstring wide(const std::string& text){
  if(text.empty())return {};
  const int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,
    text.data(),static_cast<int>(text.size()),nullptr,0);
  if(size<=0)throw std::runtime_error("Editor argument is not UTF-8");
  std::wstring out(static_cast<std::size_t>(size),L'\0');
  MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),
    static_cast<int>(text.size()),out.data(),size);
  return out;
}
std::wstring quote(const std::wstring& value){
  std::wstring out=L"\"";std::size_t slashes=0;
  for(const wchar_t c:value){
    if(c==L'\\'){++slashes;continue;}
    if(c==L'\"'){
      out.append(slashes*2+1,L'\\');out+=c;slashes=0;continue;}
    out.append(slashes,L'\\');slashes=0;out+=c;
  }
  out.append(slashes*2,L'\\');out+=L'\"';return out;
}
std::string commandText(const EditorCommand& command){
  if(command.windowsShellCommand)return *command.windowsShellCommand;
  std::string text;
  for(const auto& arg:command.argv){
    if(!text.empty())text+=' ';
    text+='"'+arg+'"';
  }
  return text;
}
ProcessResult spawn(const std::string& command,bool detached){
  if(command.empty())return {0,"Editor command is empty"};
  const auto configured=runtime::environmentUtf8("COMSPEC");
  const auto executable=wide(configured.empty()?"cmd.exe":configured);
  // cmd /s removes the outer pair; its command text is not a CRT argv string.
  auto line=quote(executable)+L" /d /s /c \""+wide(command)+L'"';
  STARTUPINFOW start{};start.cb=sizeof(start);
  start.dwFlags=STARTF_USESTDHANDLES;
  HANDLE ignored=INVALID_HANDLE_VALUE;
  if(detached){
    SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};
    ignored=CreateFileW(L"NUL",GENERIC_READ|GENERIC_WRITE,
      FILE_SHARE_READ|FILE_SHARE_WRITE,&attributes,OPEN_EXISTING,0,nullptr);
    if(ignored==INVALID_HANDLE_VALUE)return {0,"Could not prepare detached editor IO"};
  }
  start.hStdInput=detached?ignored:GetStdHandle(STD_INPUT_HANDLE);
  start.hStdOutput=detached?ignored:GetStdHandle(STD_OUTPUT_HANDLE);
  start.hStdError=detached?ignored:GetStdHandle(STD_ERROR_HANDLE);
  PROCESS_INFORMATION process{};
  const DWORD flags=detached?DETACHED_PROCESS|CREATE_NEW_PROCESS_GROUP:0;
  const bool created=CreateProcessW(executable.c_str(),line.data(),nullptr,
    nullptr,TRUE,flags,nullptr,nullptr,&start,&process);
  const auto error=created?ERROR_SUCCESS:GetLastError();
  if(ignored!=INVALID_HANDLE_VALUE)CloseHandle(ignored);
  if(!created)
    return {0,"Could not launch editor (Windows error "+
      std::to_string(error)+")"};
  CloseHandle(process.hThread);
  if(detached){CloseHandle(process.hProcess);return {};}
  WaitForSingleObject(process.hProcess,INFINITE);
  DWORD code=0;GetExitCodeProcess(process.hProcess,&code);
  CloseHandle(process.hProcess);
  return {static_cast<int>(code),{}};
}
#endif
}
std::optional<EditorCommand> externalEditor(){
  static const auto chosen=[]()->std::optional<EditorCommand>{
    std::string editor;
#ifdef _WIN32
    editor=trim(runtime::environmentUtf8("VISUAL"));
    if(editor.empty())editor=trim(runtime::environmentUtf8("EDITOR"));
#else
    if(const auto* value=std::getenv("VISUAL"))editor=trim(value);
    if(editor.empty())if(const auto* value=std::getenv("EDITOR"))editor=trim(value);
#endif
    if(editor.empty()){
#ifdef _WIN32
      editor="start /wait notepad";
#else
      for(const auto* candidate:{"code","vi","nano"})
        if(available(candidate)){editor=candidate;break;}
#endif
    }
    if(editor.empty())return {};
    auto args=splitCommand(editor);
    if(args.empty())return {};
    EditorCommand command{args,display(args.front()),isGui(args.front())};
#ifdef _WIN32
    command.windowsShellCommand=editor;
#endif
    return command;
  }();
  return chosen;
}
ProcessResult runInherited(const EditorCommand& command,
    const std::string& filePath){
#ifdef _WIN32
  auto text=commandText(command);
  if(text=="code")text+=" -w";
  else if(text=="subl")text+=" --wait";
  return spawn(text+" \""+filePath+'"',false);
#else
  auto args=command.argv;
  if(args.size()==1&&args.front()=="code")args.push_back("-w");
  else if(args.size()==1&&args.front()=="subl")args.push_back("--wait");
  args.push_back(filePath);
  return spawn(args,false);
#endif
}
bool launchDetached(const EditorCommand& command,
    const std::string& filePath,std::optional<int> line){
  if(command.argv.empty())return false;
  std::vector<std::string> args;
  const auto base=lower(basename(command.argv.front()));
  if(line&&*line>0){
    if(base=="code"||base=="cursor"||base=="windsurf"||base=="codium"){
      args.push_back("-g");args.push_back(filePath+":"+std::to_string(*line));
    }else if(base=="subl")args.push_back(filePath+":"+std::to_string(*line));
    else args.push_back(filePath);
  }else args.push_back(filePath);
#ifdef _WIN32
  auto text=commandText(command);
  for(const auto& arg:args)text+=" \""+arg+'"';
  return !spawn(text,true).error.has_value();
#else
  args.insert(args.begin(),command.argv.begin(),command.argv.end());
  return !spawn(args,true).error.has_value();
#endif
}
}
