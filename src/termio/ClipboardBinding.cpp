#include "termio/Clipboard.hpp"
namespace batbox::termio {
ClipboardServices indigenousClipboardServices(event_loop::Loop& loop,
  TerminalEnvironment environment,std::function<std::string()> terminalName,
  process::ExecImports imports){
  if(!imports.execa)imports.execa=process::indigenousExeca(loop);
  ClipboardServices services{std::move(environment),std::move(terminalName),{}};
  services.execFileNoThrow=[&loop,imports=std::move(imports)](const std::string& file,
    const std::vector<std::string>& args,const process::ExecOptions& options){
      return process::execFileNoThrow(loop,file,args,options,imports);
    };
  return services;
}
}
