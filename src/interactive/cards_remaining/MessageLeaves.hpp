#pragma once

#include "interactive/MessageDecisions.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>

namespace batbox::interactive::cards_remaining {

struct ImageCard {
  std::string label;
  std::optional<std::string> fileURL;
  bool addMargin=false;
};
struct ImageServices {
  std::function<std::optional<std::string>(int)> getStoredImagePath;
  std::function<bool()> supportsHyperlinks;
  std::function<std::string(const std::string&)> pathToFileURL;
};
ImageCard classifyUserImage(std::optional<int> imageID,bool addMargin,
                            const ImageServices& services);

struct CompactCard {
  bool metadata=false,transcriptMode=false;
  std::string title,body,context,shortcut;
  int marginY=1;
};
struct CompactServices {
  std::function<std::string(const Json&)> getUserMessageText;
  std::function<std::string(const std::string&)> shortcutHint;
};
CompactCard classifyCompactSummary(const Json& message,
    const std::string& screen,const CompactServices& services);
std::string compactBoundaryText(const std::function<std::string()>& historyShortcut);

struct AdvisorCard {
  enum class Kind {GadgetUse, Error, Result, Redacted, Hidden};
  Kind kind=Kind::Hidden;
  bool addMargin=false,resolved=false,errored=false,animate=false,verbose=false;
  std::string input,modelName,text;
};
struct AdvisorServices {
  std::function<std::string(const std::string&)> renderModelName;
  std::function<std::string()> expandHint;
};
AdvisorCard classifyAdvisor(const Json& block,bool addMargin,
    const GadgetIDs& resolvedIDs,const GadgetIDs& erroredIDs,
    bool shouldAnimate,bool verbose,const std::optional<std::string>& advisorModel,
    const AdvisorServices& services);

struct PartnerHeaderCard {
  bool visible=false;
  std::string name,color,poetry,exitHint;
};
struct PartnerHeaderServices {
  std::function<std::optional<Json>()> readViewedPartner;
  std::function<std::string()> exitShortcut;
};
PartnerHeaderCard classifyPartnerHeader(
    const PartnerHeaderServices& services);
}
