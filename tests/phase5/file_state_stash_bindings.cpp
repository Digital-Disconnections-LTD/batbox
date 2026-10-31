#include "file_state_stash/Bindings.hpp"
#include "query_engine/Ask.hpp"
#include "gadgets/files/FileEdit.hpp"
#include "gadgets/files/FileRead.hpp"
#include "gadgets/files/FileWrite.hpp"
#include "filesystem/FileOps.hpp"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <unistd.h>

using namespace batbox;
int main(){
  namespace fs=std::filesystem;
  const fs::path directory=fs::temp_directory_path()/
    ("batbox-cache-binding-"+std::to_string(getpid()));
  fs::create_directories(directory);
  const fs::path file=directory/"note.txt";
  {std::ofstream out(file);out<<"original";}

  auto stash=file_state_stash::createFileStateStashWithSizeLimit(2,32);
  const auto mtime=filesystem::getFileModificationTime(file.string());
  stash->set(file.string(),file_state_stash::FileState{"original",std::floor(mtime)});
  gadgets::files::FileReadServices read;
  gadgets::files::FileEditServices edit;
  file_state_stash::bindReadStash(read,stash);
  file_state_stash::bindWriteStash(edit,stash);
  assert(read.readFileState==edit.readFileState);
  stash.reset();
  assert(read.readFileStateOwner&&edit.readFileStateOwner);
  assert(read.readFileState->get(file.string())==
    edit.readFileState->get(file.string()));

  query_engine::AskServices ask;
  file_state_stash::bindAskStash(ask);
  auto cloned=ask.cloneFileStateStash(read.readFileStateOwner);
  assert(cloned!=read.readFileStateOwner);
  assert(cloned->get(file.string())==read.readFileState->get(file.string()));
  cloned->remove(file.string());
  assert(read.readFileState->has(file.string()));

  edit.paths.getCwd=[&]{return directory.string();};
  edit.paths.homeDirectory=[&]{return directory.string();};
  edit.paths.platform=[](){return std::string("linux");};
  edit.paths.normalizeNFC=[](const std::string& path){return path;};
  edit.checkTeamMemSecrets=[](const std::string&,const std::string&){
    return std::optional<std::string>{};};
  edit.matchingEditDenyRule=[](const std::string&){return false;};
  const auto result=gadgets::files::validateWriteInput(Json{{"file_path",file.string()},
    {"content","replacement"}},edit);
  assert(result.result);
  fs::remove(file);fs::remove(directory);
}
