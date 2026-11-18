#include "application_poetry/Provider.hpp"
#include "git/GitBindings.hpp"
#include "runtime/Platform.hpp"

namespace batbox::application_poetry {
void Provider::bindGit(){
  canonicalRoot_=std::make_shared<git::CanonicalRoot>(
    git::indigenousCanonicalRootImports());
  auto filesystem=git_filesystem::indigenousGitFsServices(owner_);
  // GitFilesystem resolves the .git entry itself, so it needs the nearest
  // root: a worktree's own directory with its gitdir-pointing .git file.
  filesystem.findGitRoot=[root=canonicalRoot_](const std::string& path){
    return root->nearest(path);
  };
  filesystem.getCwd=[this]{return cwd();};
  // GitFilesystem owns the indigenous file watches and tears them down on destruction.
  filesystem.registerCleanup=[](std::function<void()>){};
  gitFilesystem_=std::make_unique<git_filesystem::GitFilesystem>(
    owner_,std::move(filesystem));
  auto diagnostics=inputs_.logDiagnostic;
  if(!diagnostics)diagnostics=[](const std::string&,const std::string&,
      const std::optional<Json>&){};
  gitBasics_=std::make_unique<git::GitBasics>(owner_,
    git::indigenousBasicsImports(canonicalRoot_,*gitFilesystem_,
      [this]{return cwd();},std::move(diagnostics)));
  execImports_.execa=process::indigenousExeca(owner_);
  execImports_.getCwd=[this]{return cwd();};
  execImports_.logError=[this](std::exception_ptr error){
    if(inputs_.logError)inputs_.logError(error);
  };
}
} // namespace batbox::application_poetry
