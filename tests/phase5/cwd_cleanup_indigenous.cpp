#include "cwd/Override.hpp"
#include "cwd/SetCwd.hpp"
#include "cwd/State.hpp"
#include "runtime/Unicode.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unistd.h>

using namespace batbox;
int main(){
  namespace fs=std::filesystem;
  const auto root=fs::canonical(fs::temp_directory_path())/("batbox-cwd-"+std::to_string(::getpid()));
  fs::create_directories(root/"physical");
  fs::create_directory_symlink(root/"physical",root/"link");
  auto state=cwd::initialState({[&]{return (root/"link").string();},
    cwd::indigenousInitialServices().realpathSync});
  assert(state.originalCwd==(root/"physical").string());
  assert(state.projectRoot==state.originalCwd);
  assert(state.cwd==state.originalCwd);
  cwd::setCwd(state,"link",root.string(),cwd::indigenousSetCwdServices());
  assert(state.cwd==(root/"physical").string());
  cwd::setProjectRoot(state,"e\xcc\x81");
  assert(state.projectRoot==value::normalizeNFC("e\xcc\x81"));
  assert(state.cwd==(root/"physical").string());
  bool missing=false;
  try{cwd::setCwd(state,"missing",root.string(),cwd::indigenousSetCwdServices());}
  catch(const std::runtime_error& error){
    missing=std::string(error.what())=="Path \""+(root/"missing").string()+"\" does not exist";
  }
  assert(missing&&state.cwd==(root/"physical").string());
  auto wrapped=cwd::runWithCwdOverride((root/"link").string(),[&]{
    return cwd::bindCurrentCwd([&]{return cwd::pwd(state);});
  });
  assert(wrapped()==(root/"link").string());
  assert(cwd::pwd(state)==(root/"physical").string());
  fs::remove_all(root);
}
