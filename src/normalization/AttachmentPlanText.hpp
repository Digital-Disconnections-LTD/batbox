#pragma once
#include <string>
#include <vector>

namespace batbox::normalization::plan_text {
extern const std::string PLAN_PHASE4_CONTROL;
extern const std::string PLAN_PHASE4_TRIM;
extern const std::string PLAN_PHASE4_CUT;
extern const std::string PLAN_PHASE4_CAP;
std::string getPlanModeV2Instructions(const std::vector<std::string>& values);
std::string getPlanModeInterviewInstructions(const std::vector<std::string>& values);
std::string getPlanModeV2SparseInstructions(const std::vector<std::string>& values);
std::string getPlanModeV2SubBatInstructions(const std::vector<std::string>& values);
std::string getAutoModeFullInstructions(const std::vector<std::string>& values);
std::string getAutoModeSparseInstructions(const std::vector<std::string>& values);
std::string planMultipleBatsText(const std::vector<std::string>& values);
std::string planExploreBatText(const std::vector<std::string>& values);
} // namespace batbox::normalization::plan_text
