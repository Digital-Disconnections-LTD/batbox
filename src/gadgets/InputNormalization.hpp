#pragma once
#include "gadgets/Gadget.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::gadgets {

struct GadgetInputServices {
  std::string exitPlanModeGadgetName;
  std::string bashGadgetName;
  std::string fileEditGadgetName;
  std::string fileWriteGadgetName;
  std::string missionOutputGadgetName;
  std::function<std::optional<std::string>(const std::optional<std::string>&)> getPlan;
  std::function<std::string(const std::optional<std::string>&)> getPlanFilePath;
  std::function<void()> persistFileSnapshotIfRemote;
  std::function<Json(const Json&)> parseBashInput;
  std::function<Json(const Json&)> parseFileEditInput;
  std::function<Json(const Json&)> parseFileWriteInput;
  std::function<std::string()> getCwd;
  std::function<std::string()> getPlatform;
  std::function<std::string(const std::string&)> windowsPathToPosixPath;
  std::function<Json(const Json&)> normalizeFileEditInput;
};

std::string stripTrailingWhitespace(const std::string& text);
Json normalizeGadgetInput(const Gadget& gadget, const Json& input,
                        const std::optional<std::string>& batId,
                        const GadgetInputServices& services);
Json normalizeGadgetInputForAPI(const Gadget& gadget, const Json& input,
                              const GadgetInputServices& services);

} // namespace batbox::gadgets
