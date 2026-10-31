#pragma once

#include "interactive/cards_remaining/AttachmentCard.hpp"
#include <ftxui/dom/elements.hpp>

namespace batbox::interactive::cards_remaining {
struct AttachmentPalette {
  ftxui::Color text=ftxui::Color::Default;
  ftxui::Color subtle=ftxui::Color::Default;
  ftxui::Color error=ftxui::Color::Default;
  ftxui::Color warning=ftxui::Color::Default;
  ftxui::Color success=ftxui::Color::Default;
  std::string blackCircle;
};
struct AttachmentProjectionImports {
  AttachmentPalette palette;
  std::function<ftxui::Color(const Json&)> selectedBackground;
  std::function<ftxui::Color(const std::string&)> partnerColor;
  std::function<bool()> supportsHyperlinks;
  std::function<std::string(const std::string&)> filePathURL;
  std::function<std::string(const std::string&)> basename;
  std::function<std::string()> expandHint;
  std::function<ftxui::Element(const std::string&,bool,bool,bool)> renderUserText;
  std::function<ftxui::Element(int)> renderImage;
  std::function<ftxui::Element(const Json&,bool)> renderDiagnostics;
  std::function<ftxui::Element(const MailboxItem&)> renderPlanApproval;
  std::function<ftxui::Element(const MailboxItem&,bool)> renderPartnerText;
  std::function<ftxui::Element(const std::string&)> renderAnsi;
};
std::optional<ftxui::Element> projectAttachment(const AttachmentCard& card,
    const AttachmentProjectionImports& imports);
}
