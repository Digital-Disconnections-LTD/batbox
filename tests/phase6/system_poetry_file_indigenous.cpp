#include "application_commands/PoetryCommand.hpp"
#include "application_poetry/Provider.hpp"
#include "runtime/Platform.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace {
namespace fs=std::filesystem;
void expect(bool condition,const std::string& message){
  if(!condition)throw std::logic_error(message);
}
struct Fixture {
  fs::path root;
  event_loop::Loop loop;
  std::vector<std::string> log;
  std::shared_ptr<application_poetry::Provider> provider;
  explicit Fixture(fs::path directory,bool simple=false):root(std::move(directory)){
    fs::create_directories(root/"project"/".git");
    std::ofstream(root/"project"/".git"/"HEAD")<<"ref: refs/heads/main\n";
    fs::create_directories(root/"config");
    application_poetry::Inputs inputs;
    inputs.config.cwd=root/"project";
    inputs.config.stateDirectory=root/"state";
    inputs.config.configDirectory=root/"config";
    inputs.config.configPath=root/"config"/"native-client.json";
    inputs.config.model="local-model";
    inputs.environment["BATBOX_SIMPLE"]=simple?"1":"0";
    inputs.logDiagnostic=[this](const std::string& scope,const std::string& message,
        const std::optional<Json>&){log.push_back(scope+": "+message);};
    provider=std::make_shared<application_poetry::Provider>(loop,std::move(inputs));
  }
  fs::path file()const{return root/"config"/"system.md";}
  void write(const std::string& text){
    std::ofstream(file(),std::ios::binary|std::ios::trunc)<<text;
  }
  std::vector<std::string> poetry(){
    std::vector<std::string> blocks;std::exception_ptr failure;bool done=false;
    auto keep=loop.hold();
    provider->systemPoetry(Json::array({Json{{"name","Read"}},Json{{"name","TodoWrite"}}}),
      "local-model",Json::array()).then([&](const std::vector<std::string>* value,
        const std::exception_ptr& error){
      if(error)failure=error;else blocks=*value;done=true;loop.stop();});
    if(!done)loop.run();
    keep.release();
    if(failure)std::rethrow_exception(failure);
    return blocks;
  }
  std::size_t logged(const std::string& text)const{
    std::size_t count=0;
    for(const auto& line:log)if(line.find(text)!=std::string::npos)++count;
    return count;
  }
};
bool contains(const std::vector<std::string>& blocks,const std::string& text){
  for(const auto& block:blocks)if(block.find(text)!=std::string::npos)return true;
  return false;
}
void run(const fs::path& root){
  Fixture fixture(root/"main");
  const auto builtIn=fixture.poetry();
  expect(builtIn.at(0).rfind("\nYou are an interactive agent",0)==0&&
    contains(builtIn,"# Using your tools")&&fixture.logged("system poetry")==0,
    "Without system.md the built-in head should be used quietly");

  fixture.write("<!-- mine -->\nCUSTOM HEAD\n\n{{section:env_info_simple}}\n\n"
    "AFTER ENV {{section:nope}}\n");
  const auto custom=fixture.poetry();
  expect(custom.size()>=4&&custom[0]=="CUSTOM HEAD"&&
    custom[1].find("# Environment")==0&&custom[2]=="AFTER ENV {{section:nope}}"&&
    custom[3].find("# Using your tools")==0,"system.md did not replace the head in place");
  expect(!contains(custom,"interactive agent")&&!contains(custom,"<!--"),
    "The built-in head or a comment leaked into the override");
  expect(contains({custom.back()},"When working with tool results"),
    "Unreferenced sections were not appended at the end");
  std::size_t environments=0;
  for(const auto& block:custom)if(block.find("# Environment")!=std::string::npos)++environments;
  expect(environments==1,"A placed section was also appended");
  fixture.poetry();
  expect(fixture.logged("Unknown placeholder left as text: {{section:nope}}")==1&&
    fixture.logged("system poetry: using ")==1,"The unknown placeholder was not logged exactly once");

  fixture.write("EDITED HEAD SAME SIZE\n");
  expect(fixture.poetry().at(0)=="EDITED HEAD SAME SIZE","A resized edit was not picked up");
  const auto stamp=fs::last_write_time(fixture.file());
  fixture.write("EDITED HEAD SAME SIZX\n");
  fs::last_write_time(fixture.file(),stamp+std::chrono::seconds(2));
  expect(fixture.poetry().at(0)=="EDITED HEAD SAME SIZX","A same-size edit was not picked up on the next request");
  fixture.provider->resetProjectInstructionsAfterCompact();
  fixture.provider->clearPoetrySections();
  expect(fixture.poetry().at(0)=="EDITED HEAD SAME SIZX","The override did not survive compaction");

  fixture.write("<!-- nothing yet -->\n\n  \n");
  const auto empty=fixture.poetry();
  expect(empty==builtIn,"An empty system.md should leave the built-in head");
  expect(fixture.logged("is empty; using the built-in system prompt")==1,"The empty file was not logged");
  const auto& source=fixture.provider->systemPoetrySource();
  expect(!source.fromFile&&source.problem=="is empty","The empty file was not reported");
  expect(application_commands::poetryCommandText(source,false).find(
    "System poetry (system prompt): built-in\nWarning: ")==0,"/poetry hid the empty file");

  fs::remove(fixture.file());
  expect(fixture.poetry()==builtIn,"Removing system.md did not restore the built-in head");

  fixture.write("CUSTOM {{section:language}}\nsecond line\n");
  const auto text=application_commands::poetryCommandText(
    fixture.provider->systemPoetrySource(),true);
  expect(text.find("System poetry (system prompt): ")==0&&
    text.find("system.md · 40 bytes · 1 placeholder")!=std::string::npos&&
    text.find("BATBOX_SIMPLE is set")!=std::string::npos&&
    text.find("\n  CUSTOM {{section:language}}\n  second line")!=std::string::npos,
    "/poetry did not show the source and preview: "+text);

  Fixture simple(root/"simple",true);
  simple.write("CUSTOM HEAD\n");
  const auto shortPrompt=simple.poetry();
  expect(shortPrompt.size()==1&&shortPrompt[0].rfind(
    "You are Batbox, an interactive coding assistant.\n\nCWD: ",0)==0,
    "BATBOX_SIMPLE lost precedence over system.md");
}
}
int main(){
  const auto root=fs::temp_directory_path()/("batbox-system-poetry-"+platform::randomUUID());
  try{run(root);}
  catch(...){std::error_code ignored;fs::remove_all(root,ignored);throw;}
  fs::remove_all(root);
  std::cout<<"system.md override, placement, appended sections, unknown placeholder log, "
    "reload, compaction, empty file, /poetry and BATBOX_SIMPLE passed\n";
}
