#include "cli/util/Exit.hpp"
#include "cleanup/Exit.hpp"
#include <iostream>
#include <stdexcept>
namespace batbox::cli {
void cliError(const std::optional<std::string>& message,const ExitServices& services){
  if(message&&!message->empty()){
    if(!services.consoleError)throw std::invalid_argument("CLI console.error import required");
    services.consoleError(*message);
  }
  if(!services.processExit)throw std::invalid_argument("CLI process.exit import required");
  services.processExit(1);
}
void cliOk(const std::optional<std::string>& message,const ExitServices& services){
  if(message&&!message->empty()){
    if(!services.stdoutWrite)throw std::invalid_argument("CLI stdout.write import required");
    services.stdoutWrite(*message+'\n');
  }
  if(!services.processExit)throw std::invalid_argument("CLI process.exit import required");
  services.processExit(0);
}
ExitServices indigenousExitServices(){
  return {[](const std::string& text){std::cerr<<text<<std::endl;},
    [](const std::string& text){std::cout<<text;std::cout.flush();},
    cleanup::indigenousProcessExit()};
}
}
