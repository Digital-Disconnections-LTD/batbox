#include "project_recovery/Recovery.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <stdexcept>
#include <string>

namespace fs=std::filesystem;
namespace recovery=batbox::project_recovery;
namespace {
void check(bool condition,const char* message){
  if(!condition)throw std::runtime_error(message);
}
void write(const fs::path& path,const std::string& text){
  fs::create_directories(path.parent_path());
  std::ofstream file(path,std::ios::binary);
  file<<text;
  if(!file)throw std::runtime_error("Fixture write failed");
}
std::string read(const fs::path& path){
  std::ifstream file(path,std::ios::binary);
  return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
bool warningContains(const recovery::RecoveryPacket& packet,const std::string& text){
  return std::any_of(packet.warnings.begin(),packet.warnings.end(),[&](const auto& warning){
    return warning.find(text)!=std::string::npos;
  });
}
struct Fixture {
  fs::path path=fs::temp_directory_path()/("batbox-project-recovery-"+
    std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+
    std::to_string(std::random_device{}()));
  Fixture(){fs::create_directories(path);path=fs::canonical(path);}
  ~Fixture(){std::error_code error;fs::remove_all(path,error);}
};
void initializationAndDiscovery(const fs::path& base){
  const auto project=base/"git-project";
  fs::create_directories(project/".git");
  fs::create_directories(project/"src/deep");
  check(recovery::load(project/"src").text.empty(),"Fresh project has no recovery packet");
  check(!fs::exists(project/".batbox"),"Reading creates no project files");
  const auto created=recovery::init(project/"src/deep");
  check(created.projectRoot==project,"Initialization uses nearest Git root");
  check(created.created.size()==3&&created.warnings.empty(),"Initialization creates three files");
  check(!fs::exists(created.directory/"state"),"Initialization uses direct numbered files");
  const auto state=created.directory/"Makefile";
  const std::string chosen="CURRENT := connect_memory\nNEXT := Wire the loader.\n";
  write(state,chosen);
  const auto repeated=recovery::init(project/"src");
  check(repeated.created.empty()&&repeated.existing.size()==3,"Repeated initialization preserves files");
  check(read(state)==chosen,"Existing current target survives initialization");
  const auto packet=recovery::load(project/"src/deep");
  check(packet.projectRoot==project&&packet.files.size()==3,"Nested directory loads project state");
  check(packet.text.find(chosen)!=std::string::npos,"Current target enters the recovery packet");

  const auto nested=project/"nested-worktree";
  fs::create_directories(nested/"src");
  write(nested/".git","gitdir: ../../elsewhere\n");
  check(!recovery::findDirectory(nested/"src"),"Nested worktree does not inherit outer project state");
  check(recovery::init(nested/"src").projectRoot==nested,"Worktree .git file bounds initialization");
  check(recovery::load(nested/"src").projectRoot==nested,"Worktree restores its own state");

  std::error_code error;
  fs::create_directory_symlink(project,base/"project-link",error);
  if(!error)check(recovery::load(base/"project-link/src").projectRoot==project,
    "Symlinked working directory resolves to canonical project identity");
  const auto plain=base/"plain-project";
  fs::create_directories(plain/"nested");
  check(recovery::init(plain).projectRoot==plain,"Project without Git initializes at cwd");
  check(recovery::init(plain/"nested").directory==plain/".batbox",
    "Initialization reuses existing parent recovery directory without Git");
  check(recovery::init(base/"missing").created.empty(),"Missing working directory is not created");
}
void topicIndexAndBounds(const fs::path& base){
  const auto project=base/"topics";
  fs::create_directories(project);
  const auto directory=recovery::init(project).directory;
  write(directory/"30_testing.md","DETAIL_CONTENT_MUST_STAY_ON_DEMAND");
  write(directory/"20_build.md","ANOTHER_DETAIL_CONTENT");
  write(directory/"notes.md","UNNUMBERED_CONTENT");
  write(directory/"nested/40_hidden.md","NESTED_CONTENT");
  write(directory/"state/90_next_action.md","OLD_DRAFT_CONTENT");
  write(base/"outside/50_elsewhere.md","OTHER_PROJECT_CONTENT");
  const auto packet=recovery::load(project);
  check(packet.files.size()==3,"Topic files are not automatically loaded");
  const auto build=packet.text.find("- .batbox/20_build.md");
  const auto tests=packet.text.find("- .batbox/30_testing.md");
  check(build!=std::string::npos&&tests>build,"Numbered topic names are lexically indexed");
  for(const auto* text:{"DETAIL_CONTENT","ANOTHER_DETAIL","UNNUMBERED_CONTENT",
      "40_hidden","OLD_DRAFT_CONTENT","50_elsewhere"})
    check(packet.text.find(text)==std::string::npos,"Only scoped filenames enter the topic index");

  for(int index=20;index<90;++index)
    write(directory/(std::to_string(index)+"_topic.md"),"LARGE_TOPIC_CONTENT");
  write(directory/"Makefile","CURRENT := useful_target\n"+std::string(100000,'a'));
  write(directory/"00_project.md","PROJECT_ANCHOR\n"+std::string(100000,'b'));
  write(directory/"10_memory.md","MEMORY_ANCHOR\n"+std::string(100000,'c'));
  const auto bounded=recovery::load(project);
  check(bounded.text.size()<=recovery::packetByteLimit,"All labels, state and index fit the packet limit");
  check(bounded.files.size()==3,"Large early files do not crowd out later state");
  for(const auto* anchor:{"CURRENT := useful_target","PROJECT_ANCHOR","MEMORY_ANCHOR"})
    check(bounded.text.find(anchor)!=std::string::npos,"Each state file has a fair content budget");
  check(warningContains(bounded,"Truncated .batbox/Makefile"),"Oversized Makefile reports truncation");
  check(warningContains(bounded,"Truncated .batbox/10_memory.md"),"Last state file reports truncation");
  const auto index=bounded.text.find("### Numbered topics");
  check(index!=std::string::npos&&bounded.text.size()-index<=recovery::topicIndexByteLimit,
    "Large topic directory produces a bounded index");
  check(bounded.text.find("more topic files")!=std::string::npos,"Omitted topic names are acknowledged");
  check(bounded.text.find("LARGE_TOPIC_CONTENT")==std::string::npos,"Indexing reads no topic content");
}
void malformedAndLiteral(const fs::path& base){
  const auto project=base/"literal";
  fs::create_directories(project);
  const auto directory=recovery::init(project).directory;
  const auto marker=project/"recipe-ran";
  write(directory/"Makefile","CURRENT := $(shell touch "+marker.generic_string()+")\n"
    "not valid make syntax either\n");
  write(directory/"00_project.md",std::string("prefix \xc0\xaf",9)+std::string(1,'\0')+" suffix\n");
  const std::string bat="\xf0\x9f\xa6\x87";
  write(directory/"10_memory.md",std::string(2047,'x')+bat+" trailing");
  const auto packet=recovery::load(project);
  check(!fs::exists(marker),"Recovery never executes Makefile expansion or recipes");
  check(packet.text.find("$(shell touch")!=std::string::npos,"Makefile is passed through as literal state");
  check(packet.text.find("not valid make syntax either")!=std::string::npos,
    "Malformed graph syntax does not prevent recovery");
  check(packet.text.find('\0')==std::string::npos&&packet.text.find('\xc0')==std::string::npos,
    "Invalid UTF-8 and NUL bytes are replaced");
  check(packet.text.find("\xef\xbf\xbd")!=std::string::npos&&warningContains(packet,"invalid text bytes"),
    "Malformed text replacement is visible and reported");
  check(packet.text.find('\xf0')==std::string::npos,"Truncation preserves complete UTF-8 characters");
  fs::remove(directory/"00_project.md");
  fs::create_directory(directory/"00_project.md");
  check(!recovery::init(project).warnings.empty(),"Non-file default paths are preserved and reported");
  check(recovery::load(project).files.size()==2,"Unavailable state file does not prevent other recovery");
}
}
int main(){
  try{
    Fixture fixture;
    initializationAndDiscovery(fixture.path);
    topicIndexAndBounds(fixture.path);
    malformedAndLiteral(fixture.path);
    std::cout<<"project recovery: discovery, bounded state and literal loading passed\n";
    return 0;
  }catch(const std::exception& error){
    std::cerr<<error.what()<<'\n';return 1;
  }
}
