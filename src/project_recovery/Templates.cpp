#include "project_recovery/Internal.hpp"

namespace batbox::project_recovery::detail {
const std::vector<Template>& defaults(){
  static const std::vector<Template> files={
    {"Makefile",R"make(# Current project work and dependencies. Batbox reads this file as text.
# Revise CURRENT and NEXT after meaningful progress; keep the graph concise.
CURRENT := orient
NEXT := Read 00_project.md and choose the next concrete target.
.DEFAULT_GOAL := $(CURRENT)

.PHONY: orient
orient: 00_project.md 10_memory.md

# Add named targets and their prerequisites as the work takes shape.
# Put detailed instructions in numbered Markdown files and refer to them here.
# Recipes are optional; reading this recovery file runs no commands.
)make"},
    {"00_project.md",R"state(# Project

Purpose: Describe the current project outcome.
Working area: Name the relevant code and important paths.
Commands: List the few commands useful for working here.
Success: Describe the observable result we are aiming for.

This is a short current overview. Replace stale details in place; keep longer
explanations in numbered topic files such as 20_build.md or 30_architecture.md.
)state"},
    {"10_memory.md",R"state(# Current memory

Record useful facts and decisions that still guide this project, with brief reasons.
Replace superseded facts in place. Detailed notes belong in numbered topic files;
their filenames appear in the recovery index for reading when relevant.

Project memory will take shape as the work progresses.
)state"}
  };
  return files;
}
}
