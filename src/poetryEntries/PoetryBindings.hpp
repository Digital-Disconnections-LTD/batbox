#pragma once
#include "poetryEntries/Guidance.hpp"
#include "poetryEntries/SystemPoetry.hpp"

namespace batbox::poetryEntries {
void bindGuidance(SystemPoetryInputs& poetry,
  const GuidanceServices& guidance,
  const std::string& issuesExplainer,const std::string& askUserGadgetName);
}
