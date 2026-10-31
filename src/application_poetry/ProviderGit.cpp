#include "application_poetry/Provider.hpp"
#include "git/GitBindings.hpp"
#include "git_filesystem/ConfigParser.hpp"
#include "runtime/Platform.hpp"

namespace batbox::application_poetry {
void Provider::bindGit(){
  rootFinder_=git_filesystem::indigenousRootFinder();
  auto canonical=git::indigenousCanonicalRootImports();
  canonical.findGitRoot=[finder=rootFinder_](const std::string& path){
    return finder->find(path);
  };
  canonicalRoot_=std::make_unique<git::CanonicalRoot>(std::move(canonical));
  auto filesystem=git_filesystem::indigenousGitFsServices(owner_);
  filesystem.findGitRoot=[finder=rootFinder_](const std::string& path){
    return finder->find(path);
  };
  filesystem.getCwd=[this]{return cwd();};
  git_filesystem::bindIndigenousConfigParser(owner_,filesystem);
  filesystem.waitForScrollIdle=[this]{
    event_loop::Deferred<event_loop::Unit> done(owner_);done.resolve({});
    return done;
  };
  // GitFilesystem owns the indigenous file watches and tears them down on destruction.
  filesystem.registerCleanup=[](std::function<void()>){};
  gitFilesystem_=std::make_unique<git_filesystem::GitFilesystem>(
    owner_,std::move(filesystem));
  auto diagnostics=inputs_.logDiagnostic;
  if(!diagnostics)diagnostics=[](const std::string&,const std::string&,
      const std::optional<Json>&){};
  gitBasics_=std::make_unique<git::GitBasics>(owner_,
    git::indigenousBasicsImports(owner_,rootFinder_,*gitFilesystem_,
      [this]{return cwd();},std::move(diagnostics)));
  execImports_.execa=process::indigenousExeca(owner_);
  execImports_.getCwd=[this]{return cwd();};
  execImports_.logError=[this](std::exception_ptr error){
    if(inputs_.logError)inputs_.logError(error);
  };
}
} // namespace batbox::application_poetry
