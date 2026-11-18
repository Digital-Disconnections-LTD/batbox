#pragma once
#include "context/AsyncContext.hpp"
#include "git/GitBasics.hpp"
#include "git/CanonicalRoot.hpp"
#include "git/filesystem/GitFilesystem.hpp"
#include <memory>

namespace batbox::git {
BasicsImports indigenousBasicsImports(std::shared_ptr<CanonicalRoot> finder,
  git::filesystem::GitFilesystem& filesystem,
  std::function<std::string()> getCwd,
  std::function<void(const std::string&,const std::string&,
    const std::optional<Json>&)> logDiagnostic);
// GitBasics must outlive callbacks held by AsyncServices.
void bindGitContextServices(context::AsyncServices& context,GitBasics& basics);
} // namespace batbox::git
