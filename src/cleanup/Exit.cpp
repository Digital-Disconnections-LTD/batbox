#include "cleanup/Exit.hpp"
#include <stdexcept>

namespace batbox::cleanup {
void forceExit(int code,const ExitServices& services){
  if(!services.clearFailsafe||!services.drainStdin||!services.processExit||
    !services.killSelf||!services.isTestMode)
    throw std::invalid_argument("forceExit imports required");
  services.clearFailsafe();
  try{services.drainStdin();}catch(...){}
  try{services.processExit(code);}
  catch(...){
    if(services.isTestMode())throw;
    services.killSelf();
  }
  if(!services.isTestMode())throw std::runtime_error("unreachable");
}
} // namespace batbox::cleanup
