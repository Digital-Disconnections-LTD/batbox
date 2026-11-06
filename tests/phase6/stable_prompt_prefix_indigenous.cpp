#include "application_poetry/Provider.hpp"
#include "project_recovery/Recovery.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/Platform.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace fs=std::filesystem;
namespace {
struct Prefix {std::vector<std::string> poetry;Json user;};
void require(bool condition,const std::string& message){
  if(!condition)throw std::runtime_error(message);
}
void write(const fs::path& path,const std::string& text){
  std::ofstream output(path,std::ios::binary|std::ios::trunc);
  output<<text;
  if(!output)throw std::runtime_error("Cannot write "+path.string());
}
bool has(const std::string& text,const std::string& part){
  return text.find(part)!=std::string::npos;
}
bool poetryHas(const Prefix& prefix,const std::string& part){
  for(const auto& block:prefix.poetry)if(has(block,part))return true;
  return false;
}
// The two calls the main loop makes before each turn, in its order.
Prefix prefix(event_loop::Loop& loop,application_poetry::Provider& provider,
    const std::string& model="local-model"){
  Prefix result;std::exception_ptr failure;int done=0;
  auto keep=loop.hold();
  provider.noteMainModel(model);
  provider.systemPoetry(Json::array({Json{{"name","Read"}}}),model,Json::array()).then(
    [&](const std::vector<std::string>* value,const std::exception_ptr& error){
      if(error)failure=error;else result.poetry=*value;
      if(++done==2)loop.stop();
    });
  provider.userContext().then([&](const Json* value,const std::exception_ptr& error){
    if(error)failure=error;else result.user=*value;
    if(++done==2)loop.stop();
  });
  loop.run();keep.release();
  if(failure)std::rethrow_exception(failure);
  require(done==2,"Prompt prefix did not settle");
  return result;
}
std::string head(const Prefix& prefix){
  return prefix.user.dump();
}
std::string attachmentText(const Json& attachment){
  return attachment.at("content").at("content").get<std::string>();
}
std::shared_ptr<application_poetry::Provider> makeProvider(event_loop::Loop& loop,
    const application::Config& config,Json settings=Json::object()){
  application_poetry::Inputs inputs;inputs.config=config;
  inputs.initialSettings=[settings]{return settings;};
  return std::make_shared<application_poetry::Provider>(loop,std::move(inputs));
}
}
int main(){
  const auto root=fs::temp_directory_path()/("batbox-stable-prefix-"+platform::randomUUID());
  struct Cleanup{fs::path path;~Cleanup(){std::error_code error;fs::remove_all(path,error);}}
    cleanup{root};
  fs::create_directories(root/".git");
  fs::create_directories(root/"config");
  write(root/".git/HEAD","ref: refs/heads/main\n");
  write(root/".git/config","[core]\n\trepositoryformatversion = 0\n");
  write(root/"BATBOX.md","Rule one.\n");
  project_recovery::init(root);
  write(root/".batbox/Makefile","CURRENT := first_step\n");
  event_loop::Loop loop;
  application::Config config;
  config.cwd=fs::canonical(root);
  config.stateDirectory=config.cwd/"native-state";
  config.configDirectory=config.cwd/"config";
  config.model="local-model";
  const auto base=config.cwd;
  auto provider=makeProvider(loop,config);
  require(provider->stablePromptPrefix(),"stablePromptPrefix is not on by default");
  instructions::MemoryPathStash paths;
  const auto memoryIndex=runtime::pathFromUtf8(instructions::getAutoMemEntrypoint(
    provider->memoryPathServices(),paths));
  fs::create_directories(memoryIndex.parent_path());
  write(memoryIndex,"- [Build notes](build.md): Memory index marker.\n");

  const auto first=prefix(loop,*provider);
  require(has(head(first),"Rule one.")&&has(head(first),"CURRENT := first_step"),
    "Head lacks project instructions or recovery notes");
  require(poetryHas(first,"Memory index marker.")||has(head(first),"Memory index marker."),
    "The prompt prefix lacks the memory index");
  require(provider->takeHeadChangeAttachments().empty(),"A fresh head reported changes");

  write(base/"BATBOX.md","Rule one.\nRule two from another window.\n");
  write(memoryIndex,"- [Build notes](build.md): Memory index marker.\n"
    "- [Deploy](deploy.md): Second memory marker.\n");
  write(base/"config/system.md","Custom head marker.\n");
  const auto second=prefix(loop,*provider);
  require(second.poetry==first.poetry,"System poetry bytes changed after a memory edit");
  require(head(second)==head(first),"Head message bytes changed after a memory edit");

  const auto changes=provider->takeHeadChangeAttachments();
  require(changes.size()==2,"Expected one attachment per edited head file, got "+
    std::to_string(changes.size()));
  bool sawInstructions=false,sawMemory=false;
  for(const auto& change:changes){
    require(change.at("type")=="nested_memory","Head change is not a nested_memory attachment");
    const auto path=change.at("path").get<std::string>();
    const auto text=attachmentText(change);
    if(path.ends_with("BATBOX.md")){
      sawInstructions=has(text,"Rule two from another window.")&&
        change.at("displayPath")=="BATBOX.md";
    }else if(path==runtime::pathToUtf8(fs::weakly_canonical(memoryIndex))){
      sawMemory=has(text,"Second memory marker.");
    }
  }
  require(sawInstructions&&sawMemory,"Head change attachments lack the new contents");
  require(provider->takeHeadChangeAttachments().empty(),"A head change was attached twice");

  write(base/".batbox/00_project.md","# Project\nModel progress note.\n");
  provider->noteOwnWrite(runtime::pathToUtf8(base/".batbox/00_project.md"));
  require(provider->takeHeadChangeAttachments().empty(),
    "The model's own Write produced a tail attachment");
  write(base/".batbox/10_memory.md","# Current memory\nEdited by hand.\n");
  const auto recovery=provider->takeHeadChangeAttachments();
  require(recovery.size()==1&&has(attachmentText(recovery[0]),"Edited by hand.")&&
    recovery[0].at("displayPath")==".batbox/10_memory.md",
    "An external recovery-note edit did not reach the tail");
  require(head(prefix(loop,*provider))==head(first),
    "Recovery note edits changed the head message");
  write(base/"BATBOX.md","Rule one.\nRule two from another window.\n");
  require(provider->takeHeadChangeAttachments().empty(),
    "Rewriting identical bytes produced an attachment");

  std::string large;
  for(int line=0;line<300;++line)large+="Local rule line "+std::to_string(line)+".\n";
  write(base/"BATBOX.local.md",large);
  const auto created=provider->takeHeadChangeAttachments();
  require(created.size()==1&&has(attachmentText(created[0]),"was created")&&
    has(attachmentText(created[0]),"Local rule line 299."),"A new head file was not attached");
  auto edited=large;
  edited.replace(edited.find("Local rule line 100."),20,"Local rule line one hundred.");
  write(base/"BATBOX.local.md",edited);
  const auto diff=provider->takeHeadChangeAttachments();
  require(diff.size()==1,"A large head file edit was not attached");
  const auto diffText=attachmentText(diff[0]);
  require(has(diffText,"unified diff")&&has(diffText,"-Local rule line 100.")&&
    has(diffText,"+Local rule line one hundred.")&&!has(diffText,"Local rule line 3."),
    "A large head file edit was not sent as a diff");

  const auto switched=prefix(loop,*provider,"other-model");
  require(has(head(switched),"Rule two from another window.")&&
    has(head(switched),"Edited by hand.")&&poetryHas(switched,"Custom head marker.")&&
    (poetryHas(switched,"Second memory marker.")||has(head(switched),"Second memory marker.")),"A model switch did not rebuild the prefix");
  require(provider->takeHeadChangeAttachments().empty(),
    "A rebuilt prefix still reported old changes");

  write(base/"BATBOX.md","Rule three after compaction.\n");
  provider->resetProjectInstructionsAfterCompact();
  provider->clearPoetrySections();
  const auto compacted=prefix(loop,*provider,"other-model");
  require(has(head(compacted),"Rule three after compaction."),
    "Compaction did not rebuild the head");
  require(provider->takeHeadChangeAttachments().empty(),
    "Compaction left a stale change attachment");

  write(base/"BATBOX.md","Rule four after clear.\n");
  provider->clearProjectInstructions();
  provider->clearPoetrySections();
  require(has(head(prefix(loop,*provider,"other-model")),"Rule four after clear."),
    "/clear did not rebuild the head");

  write(base/"BATBOX.md","Rule five before resume.\n");
  auto resumed=makeProvider(loop,config);
  require(has(head(prefix(loop,*resumed)),"Rule five before resume."),
    "A resumed Flight did not build a current head");

  auto rebuilding=makeProvider(loop,config,Json{{"stablePromptPrefix",false}});
  require(!rebuilding->stablePromptPrefix(),"stablePromptPrefix false was ignored");
  const auto before=prefix(loop,*rebuilding);
  write(base/"BATBOX.md","Rule six with the setting off.\n");
  const auto after=prefix(loop,*rebuilding);
  require(head(before)!=head(after)&&has(head(after),"Rule six with the setting off."),
    "With stablePromptPrefix off the head did not follow the file");
  require(rebuilding->takeHeadChangeAttachments().empty(),
    "With stablePromptPrefix off a tail attachment was produced");
  std::cout<<"Stable prompt prefix, tail attachments, own writes, rebuild points and the off switch passed\n";
}
