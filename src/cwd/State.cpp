#include "cwd/State.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/Unicode.hpp"
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#ifndef _WIN32
#include <unistd.h>
#endif

namespace batbox::cwd {
State initialState(const InitialServices& services){
  if(!services.processCwd||!services.realpathSync)
    throw std::invalid_argument("cwd initialization imports required");
  const auto raw=services.processCwd();
  std::string resolved;
  try{resolved=value::normalizeNFC(services.realpathSync(raw));}
  catch(...){resolved=value::normalizeNFC(raw);}
  return {resolved,resolved,resolved};
}
InitialServices indigenousInitialServices(){
  return {
    []{return runtime::pathToUtf8(std::filesystem::current_path());},
    [](const std::string& path){
#ifdef _WIN32
      return runtime::pathToUtf8(std::filesystem::canonical(runtime::pathFromUtf8(path)));
#else
      const auto encoded=value::encodeUtf8(path);
      char* resolved=::realpath(encoded.c_str(),nullptr);
      if(!resolved)throw std::system_error(errno,std::generic_category(),"realpath");
      std::string value(resolved);std::free(resolved);return value;
#endif
    }
  };
}
const std::string& getOriginalCwd(const State& state){return state.originalCwd;}
const std::string& getProjectRoot(const State& state){return state.projectRoot;}
const std::string& getCwdState(const State& state){return state.cwd;}
void setOriginalCwd(State& state,const std::string& value){state.originalCwd=value::normalizeNFC(value);}
void setProjectRoot(State& state,const std::string& value){state.projectRoot=value::normalizeNFC(value);}
void setCwdState(State& state,const std::string& value){state.cwd=value::normalizeNFC(value);}
} // namespace batbox::cwd
