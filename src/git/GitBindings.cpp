#include "git/GitBindings.hpp"
#include "runtime/Platform.hpp"
#include <cstdlib>
#include <stdexcept>
#include <utility>

namespace batbox::git {
BasicsImports indigenousBasicsImports(std::shared_ptr<git_filesystem::RootFinder> finder,
  git_filesystem::GitFilesystem& filesystem,
  std::function<std::string()> getCwd,
  std::function<void(const std::string&,const std::string&,
    const std::optional<Json>&)> logDiagnostic){
  if(!finder||!getCwd||!logDiagnostic)
    throw std::invalid_argument("Native Git basics imports required");
  BasicsImports imports;
  imports.nowMillis=platform::nowMillis;
  imports.logDiagnostic=std::move(logDiagnostic);
  imports.getCwd=std::move(getCwd);
  imports.findGitRoot=[finder=std::move(finder)](const std::string& path){
    return finder->find(path);};
  imports.getStashedBranch=[&filesystem]{return filesystem.getStashedBranch();};
  imports.getStashedDefaultBranch=[&filesystem]{return filesystem.getStashedDefaultBranch();};
  imports.whichSync=[](const std::string& name){
    const char* current=std::getenv("PATH");
    return indigenousWhichSync(current?current:"")(name);
  };
  return imports;
}
void bindGitContextServices(context::AsyncServices& services,GitBasics& basics){
  services.getIsGit=[&basics]{return basics.getIsGit();};
  services.getBranch=[&basics]{return basics.getBranch();};
  services.getDefaultBranch=[&basics]{return basics.getDefaultBranch();};
}
} // namespace batbox::git
