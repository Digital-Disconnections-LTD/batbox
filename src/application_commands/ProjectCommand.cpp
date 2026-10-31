#include "application_commands/ProjectCommand.hpp"
#include "project_recovery/Recovery.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <array>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace batbox::application_commands {
namespace {
namespace fs=std::filesystem;
constexpr std::array<const char*,3> recoveryFiles={"Makefile",
  "00_project.md","10_memory.md"};
const char* usage="Usage: /project [status|init|reload]";

void appendWarnings(std::ostringstream& text,
    const std::vector<std::string>& warnings){
  constexpr std::size_t maximum=5;
  for(std::size_t i=0;i<std::min(warnings.size(),maximum);++i)
    text<<"\nWarning: "<<warnings[i];
  if(warnings.size()>maximum)
    text<<"\n"<<warnings.size()-maximum<<" additional warnings.";
}

std::string status(const project_recovery::RecoveryPacket& packet,
    const fs::path& cwd){
  std::ostringstream text;
  if(packet.directory.empty()){
    text<<"Project recovery: not initialized.\nWorking directory: "
      <<runtime::pathToUtf8(cwd)
      <<"\nRun /project init to create the current-state files.";
    appendWarnings(text,packet.warnings);
    return text.str();
  }
  text<<"Project: "<<runtime::pathToUtf8(packet.projectRoot)
    <<"\nRecovery directory: "<<runtime::pathToUtf8(packet.directory)
    <<"\nRecovery packet: "<<packet.text.size()<<"/"
    <<project_recovery::packetByteLimit<<" bytes, "<<packet.files.size()
    <<"/"<<recoveryFiles.size()<<" files loaded";
  for(const auto* name:recoveryFiles){
    const auto path=packet.directory/name;
    text<<"\n  "<<name<<": ";
    if(std::find(packet.files.begin(),packet.files.end(),path)==packet.files.end()){
      text<<"not loaded";
      continue;
    }
    text<<"loaded";
    std::error_code error;
    const auto bytes=fs::file_size(path,error);
    if(!error)text<<", "<<bytes<<" bytes on disk";
  }
  appendWarnings(text,packet.warnings);
  return text.str();
}

std::string run(const std::string& action,const fs::path& cwd){
  std::string prefix;
  if(action=="init"){
    const auto initialized=project_recovery::init(cwd);
    std::ostringstream text;
    text<<"Project recovery initialization: "<<initialized.created.size()
      <<" files created, "<<initialized.existing.size()<<" existing files kept.";
    appendWarnings(text,initialized.warnings);
    prefix=text.str()+"\n";
  }else if(action=="reload")prefix="Project recovery reloaded.\n";
  return prefix+status(project_recovery::load(cwd),cwd);
}
}

user_input::CommandEntry makeProjectCommand(event_loop::Loop& owner,
    fs::path cwd,std::function<void()> refresh){
  user_input::CommandEntry entry;
  entry.descriptor={{"type","local"},{"name","project"},
    {"description","Show, initialize or reload project recovery files"},
    {"argumentHint","[status|init|reload]"},{"supportsNonInteractive",true}};
  entry.loadLocal=[&owner,cwd=std::move(cwd),refresh=std::move(refresh)]{
    event_loop::Deferred<user_input::LocalCall> loaded(owner);
    loaded.resolve([&owner,cwd,refresh](const std::string& args){
      event_loop::Deferred<Json> output(owner);
      try{
        const auto action=value::trim(args);
        if(!action.empty()&&action!="status"&&action!="init"&&action!="reload"){
          output.resolve(Json{{"type","text"},{"value",usage}});
          return output;
        }
        auto text=std::make_shared<std::string>();
        owner.queueWork([text,cwd,action]{*text=run(action,cwd);},
          [text,output,refresh,action](std::exception_ptr error){
            if(error){output.reject(error);return;}
            try{
              if(refresh&&(action=="init"||action=="reload"))refresh();
              output.resolve(Json{{"type","text"},{"value",std::move(*text)}});
            }catch(...){output.reject(std::current_exception());}
          });
      }catch(...){output.reject(std::current_exception());}
      return output;
    });
    return loaded;
  };
  return entry;
}
} // namespace batbox::application_commands
