// The permission dialog names a file by its path inside the Roost, or under
// "~" outside it, while the rule it offers to save keeps the exact path.
#include "application_permissions/PermissionProvider.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/Platform.hpp"
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>

using namespace batbox;
namespace fs=std::filesystem;

namespace {
void check(bool condition,const std::string& reason){
  if(!condition)throw std::runtime_error(reason);
}

struct Asked {
  interactive::UiRequest request;
};

// Shows one interactive request and cancels it; returns what the dialog drew.
Asked ask(const std::string& gadget,const Json& input,
    std::function<std::string()> cwd,std::function<std::string()> home){
  event_loop::Loop loop;
  std::optional<interactive::UiRequest> shown;
  auto ui=std::make_shared<interactive::CommandUi>(loop,interactive::CommandUi::Callbacks{
    [&](std::optional<interactive::UiRequest> request){if(request)shown=request;},
    [](const interactive::CommandDescriptor&,const interactive::CommandCompletion&,bool){},
    [](const std::string&){},[](bool){},[](bool){}});
  application_permissions::PermissionProvider::Inputs inputs(loop);
  inputs.ui=ui;
  inputs.workingDirectory=std::move(cwd);
  inputs.homeDirectory=std::move(home);
  auto provider=application_permissions::PermissionProvider::create(std::move(inputs));
  auto decision=provider->check(gadget,input,Json{{"mode","default"}});
  for(int i=0;i<20&&!shown;++i)loop.runReady();
  check(shown.has_value(),gadget+" request did not reach the dialog");
  provider->cancelAll();
  for(int i=0;i<20;++i)loop.runReady();
  check(decision.wait_for(std::chrono::milliseconds(0))==std::future_status::ready,
    "Cancelling must settle the request");
  return {*shown};
}

std::string text(const fs::path& path){return runtime::pathToUtf8(path);}
}

int main(){
  const auto root=fs::temp_directory_path()/("batbox-permission-path-"+platform::randomUUID());
  try{
    fs::create_directories(root/"home"/"roost"/"src");
    fs::create_directories(root/"home"/"notes");
    fs::create_directories(root/"elsewhere");
    const auto home=root/"home",roost=home/"roost";
    const auto cwd=[&]{return text(roost);};
    const auto homeText=[&]{return text(home);};

    const auto file=text(roost/"src"/"lexer.cpp");
    const auto inside=ask("Edit",Json{{"file_path",file},{"old_string","a"},{"new_string","b"}},
      cwd,homeText);
    check(inside.request.body=="src/lexer.cpp",
      "A file in the Roost must show its relative path, got: "+inside.request.body);
    check(inside.request.choiceInfo.size()>1&&
      inside.request.choiceInfo[1].detail.find("Edit("+file+")")!=std::string::npos,
      "The saved rule must keep the exact path: "+
        (inside.request.choiceInfo.size()>1?inside.request.choiceInfo[1].detail:std::string{}));

    const auto notes=ask("Write",Json{{"file_path",text(home/"notes"/"todo.md")},
      {"content","x"}},cwd,homeText);
    check(notes.request.body=="~/notes/todo.md",
      "A file under home must show a ~ path, got: "+notes.request.body);

    const auto outside=text(root/"elsewhere"/"data.txt");
    const auto far=ask("Read",Json{{"file_path",outside}},cwd,homeText);
    check(far.request.body==fs::path(outside).lexically_normal().generic_string(),
      "A file outside the Roost and home keeps its full path, got: "+far.request.body);

    // The Roost may be recorded through a symlink (macOS /var and /private/var).
    std::error_code error;
    const auto resolved=fs::weakly_canonical(roost,error);
    check(!error,"Could not resolve the test Roost");
    const auto viaLink=ask("Edit",Json{{"file_path",text(resolved/"src"/"lexer.cpp")},
      {"old_string","a"},{"new_string","b"}},cwd,homeText);
    check(viaLink.request.body=="src/lexer.cpp",
      "A resolved path inside the Roost must still be relative, got: "+viaLink.request.body);

    const auto command=ask("Bash",Json{{"command","cat "+file}},cwd,homeText);
    check(command.request.body=="cat "+file,"Shell commands are shown as written");

    const auto unbound=ask("Edit",Json{{"file_path",file},{"old_string","a"},
      {"new_string","b"}},{},{});
    check(unbound.request.body==file,"Without a Roost the path is shown unchanged");
  }catch(const std::exception& failure){
    std::error_code ignored;fs::remove_all(root,ignored);
    std::cerr<<failure.what()<<"\n";
    return 1;
  }
  std::error_code ignored;fs::remove_all(root,ignored);
  std::cout<<"Permission dialogs show file paths relative to the Roost or home\n";
  return 0;
}
