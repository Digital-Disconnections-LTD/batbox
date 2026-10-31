#include <cstdlib>
#include "gadget_results/Budget.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sys/stat.h>
#include <unistd.h>
using namespace batbox;
namespace tr=gadget_results;
template<class T>T wait(event_loop::Loop& loop,event_loop::Deferred<T> pending){
  std::optional<T> result;std::exception_ptr error;
  pending.then([&](const T* value,const auto& failure){error=failure;if(value)result=*value;});
  loop.run();if(error)std::rethrow_exception(error);assert(result);return *result;
}
std::string read(const std::string& path){std::ifstream stream(path);return {std::istreambuf_iterator<char>(stream),{}};}
int main(){
  char directory[]="/tmp/batbox-tool-result-XXXXXX";assert(::mkdtemp(directory));
  const std::string root=directory;event_loop::Loop loop;tr::StorageImports imports;
  imports.getOriginalCwd=[] {return "/work";};imports.getFlightId=[] {return "session";};
  imports.getProjectDir=[&](const auto& cwd){assert(cwd=="/work");return root;};
  imports=tr::indigenousStorageImports(loop,std::move(imports));
  const auto first=wait(loop,tr::persistGadgetResult(loop,imports,"first text","same"));
  assert(!tr::isPersistError(first));const auto path=first.at("filepath").get<std::string>();assert(read(path)=="first text");
  struct stat info{};assert(::stat(path.c_str(),&info)==0);
  const auto mask=::umask(0);::umask(mask);assert((info.st_mode&0777)==(0666&~mask));
  const auto replay=wait(loop,tr::persistGadgetResult(loop,imports,"second text","same"));
  assert(read(path)=="first text");assert(replay.at("preview")=="second text");
  Json blocks=Json::array({Json{{"type","text"},{"text","hello😀"},{"n",1e-7}}});
  const auto array=wait(loop,tr::persistGadgetResult(loop,imports,blocks,"array"));
  assert(read(array.at("filepath"))==value::stringify(blocks,2));
  assert(array.at("originalSize")==value::stringLength(value::stringify(blocks,2)));
  const auto rejected=wait(loop,tr::persistGadgetResult(loop,imports,Json::array({Json{{"type","image"}}}),"image"));
  assert(tr::isPersistError(rejected));assert(!std::filesystem::exists(tr::getGadgetResultPath(imports,"image",true)));
  // Both exclusive writes may race; EEXIST is a successful replay, and the
  // file always contains one complete value rather than mixed/truncated bytes.
  auto a=tr::persistGadgetResult(loop,imports,std::string(100000,'a'),"race");
  auto b=tr::persistGadgetResult(loop,imports,std::string(100000,'b'),"race");
  std::optional<Json> ar,br;
  a.then([&](const Json* v,const auto& e){assert(!e);ar=*v;});
  b.then([&](const Json* v,const auto& e){assert(!e);br=*v;});loop.run();assert(ar&&br);
  const auto raced=read(ar->at("filepath"));assert(raced==std::string(100000,'a')||raced==std::string(100000,'b'));
  // Selected IDs become seen only together with the completed replacement;
  // unchanged messages and the no-change message array retain shared identity.
  auto state=tr::createContentReplacementState();tr::BudgetImports budget;budget.storage=imports;budget.limitOverride=5;
  auto messages=tr::makeMessageArray(Json::array({Json{{"type","assistant"},{"message",Json{{"id","a"},{"content",Json::array()}}}},
    Json{{"type","user"},{"message",Json{{"content",Json::array({Json{{"type","tool_result"},{"tool_use_id","large"},{"content","large tool output"}}})}}}}}));
  auto pending=tr::enforceGadgetResultBudget(loop,budget,messages,state);assert(!state->seenIds.contains("large"));
  const auto result=wait(loop,pending);assert(state->seenIds.contains("large")&&state->replacements.contains("large"));
  assert(result.messages!=messages);assert((*result.messages)[0]==(*messages)[0]);assert((*result.messages)[1]!=(*messages)[1]);
  assert(result.newlyReplaced.size()==1);
  // Resume uses the exact stashed replacement, with no dependence on the file
  // still existing or the current preview implementation/size threshold.
  auto resumed=tr::reconstructForSubagentResume(state,tr::snapshotMessages(messages),result.newlyReplaced);
  std::filesystem::remove_all(root);
  auto reapplied=wait(loop,tr::enforceGadgetResultBudget(loop,budget,messages,resumed));
  assert(reapplied.newlyReplaced.empty());assert(tr::snapshotMessages(reapplied.messages)==tr::snapshotMessages(result.messages));
  auto empty=tr::makeMessageArray(Json::array());assert(wait(loop,tr::enforceGadgetResultBudget(loop,budget,empty,state)).messages==empty);
}
