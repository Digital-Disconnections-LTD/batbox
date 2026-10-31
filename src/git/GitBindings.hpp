#pragma once
#include "context/AsyncContext.hpp"
#include "git/GitBasics.hpp"
#include "git_filesystem/GitFilesystem.hpp"
#include "git_filesystem/RootFinder.hpp"
#include <memory>

namespace batbox::git {
BasicsImports indigenousBasicsImports(event_loop::Loop& loop,
  std::shared_ptr<git_filesystem::RootFinder> finder,
  git_filesystem::GitFilesystem& filesystem,
  std::function<std::string()> getCwd,
  std::function<void(const std::string&,const std::string&,
    const std::optional<Json>&)> logDiagnostic);
// GitBasics must outlive callbacks held by AsyncServices.
void bindGitContextServices(context::AsyncServices& context,GitBasics& basics);
} // namespace batbox::git
