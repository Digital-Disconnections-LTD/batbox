#include "git/GitignoreUpdate.hpp"
#include "git_filesystem/RootFinder.hpp"
#include "settings/persistence/Update.hpp"
#include "runtime/JsonValue.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace {
using namespace std::chrono_literals;
void need(bool okay,const char* message){
  if(!okay)throw std::runtime_error(message);
}
void write(const std::filesystem::path& path,const std::string& content){
  std::ofstream file(path,std::ios::binary|std::ios::trunc);
  if(!file)throw std::runtime_error("Cannot write gitignore fixture");
  file<<content;
}
std::string read(const std::filesystem::path& path){
  std::ifstream file(path,std::ios::binary);
  if(!file)throw std::runtime_error("Cannot read gitignore fixture");
  return {std::istreambuf_iterator<char>(file),{}};
}
}
int main(){
  try{
    using namespace batbox;
    using namespace batbox::git;
    using namespace batbox::settings;
    namespace persist=batbox::settings::persistence;
    char directory[]="/tmp/batbox-gitignore-real-XXXXXX";
    need(::mkdtemp(directory)!=nullptr,"mkdtemp failed");
    const auto root=std::filesystem::path(directory);
    const auto home=root/"home",repo=root/"repo",outside=root/"outside";
    std::filesystem::create_directories(home);
    std::filesystem::create_directories(repo);
    std::filesystem::create_directories(outside);
    ::setenv("HOME",home.c_str(),1);
    const auto xdg=home/".config";
    ::setenv("XDG_CONFIG_HOME",xdg.c_str(),1);
    const auto child=::fork();
    if(child==0){
      ::execlp("git","git","init","--quiet",repo.c_str(),nullptr);
      ::_exit(127);
    }
    need(child>0,"fork failed");
    int status=0;
    need(::waitpid(child,&status,0)==child&&WIFEXITED(status)&&
      WEXITSTATUS(status)==0,"git init failed");
    event_loop::Loop loop;
    auto finder=git_filesystem::indigenousRootFinder();
    BasicsImports basicImports;
    basicImports.findGitRoot=[finder](const std::string& path){
      return finder->find(path);};
    GitBasics basics(loop,basicImports);
    process::ExecImports processImports;
    processImports.execa=process::indigenousExeca(loop);
    processImports.getCwd=[&](){return repo.string();};
    int errors=0;
    processImports.logError=[&](std::exception_ptr){++errors;};
    auto gitignore=bindGitignoreServices(loop,basics,processImports,
      [&](){return repo.string();},[&](std::exception_ptr){++errors;});
    PathServices pathServices;
    pathServices.processCwd=repo.string();
    pathServices.originalCwd=repo.string();
    pathServices.configHome=(home/".batbox").string();
    pathServices.platform="linux";
    Paths paths(pathServices);
    Stash stash;
    persist::InternalWrites internal([](){return 0.0;});
    persist::SafeJsonParser safeJson([](const std::string&){});
    ParseServices parser;
    parser.filterInvalidPermissionRules=[](Json&,const std::string&){
      return Json::array();};
    parser.validateSettingsSchema=[](const Json& data){
      return SchemaResult{true,data,Json::array()};};
    parser.formatZodError=[](const Json&,const std::string&){
      return Json::array();};
    persist::UpdateServices update;
    update.paths=&paths;update.stash=&stash;
    update.internalWrites=&internal;update.safeJsonParser=&safeJson;
    update.parser=parser;update.originalCwd=repo.string();
    update.scheduleGitignore=makeSettingsGitignoreScheduler(loop,gitignore);
    persist::UpdatePatch patch;patch.value={{"model","standard"}};
    const auto result=persist::updateSettingsForSource(Source::Local,
      patch,update);
    need(!result.error,"Settings update failed");
    const auto global=home/".config"/"git"/"ignore";
    const auto expected="**/.batbox/settings.local.json\n";
    for(int i=0;i<300&&!std::filesystem::exists(global);i++){
      loop.runReady();std::this_thread::sleep_for(10ms);
    }
    for(int i=0;i<300&&std::filesystem::exists(global)&&
        read(global)!=expected;i++){
      loop.runReady();std::this_thread::sleep_for(10ms);
    }
    need(std::filesystem::exists(global)&&read(global)==expected,
      "Global gitignore entry not created");
    auto await=[&](const std::string& name,const std::string& cwd){
      bool done=false;
      const auto pending=addFileGlobRuleToGitignore(loop,name,cwd,gitignore);
      pending.then([&](const event_loop::Unit*,const std::exception_ptr& error){
        if(error)std::rethrow_exception(error);
        done=true;
      });
      for(int i=0;i<300&&!done;i++){
        loop.runReady();std::this_thread::sleep_for(10ms);
      }
      need(done,"Gitignore operation did not settle");
    };
    await(".batbox/settings.local.json",repo.string());
    need(read(global)==expected,"Duplicate gitignore entry appended");
    std::filesystem::remove(global);
    write(repo/".gitignore","**/other.json\n");
    await("other.json",repo.string());
    need(!std::filesystem::exists(global),
      "Local ignore rule was not honored");
    await("outside.json",outside.string());
    need(!std::filesystem::exists(global),
      "Outside-repo call wrote global ignore");
    need(errors==0,"Gitignore operation logged unexpected error");
    const auto settingsFile=*paths.getSettingsFilePathForSource(Source::Local);
    need(std::filesystem::exists(settingsFile),
      "Local settings file was not written");
    std::filesystem::remove_all(root);
    std::cout<<value::stringify({{"updatedLocalSettings",true},
      {"createdGlobalIgnore",true},{"avoidedDuplicate",true},
      {"honoredLocalIgnore",true},{"outsideRepoSkipped",true}})<<'\n';
  }catch(const std::exception& error){
    std::cerr<<error.what()<<'\n';return 1;
  }
}
