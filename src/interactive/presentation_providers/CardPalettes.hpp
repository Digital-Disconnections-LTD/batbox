#pragma once

#include "interactive/cards/OutputLineProjection.hpp"
#include "interactive/cards/UserTextProjection.hpp"
#include "interactive/cards_assistant/AssistantProjection.hpp"
#include "interactive/cards_remaining/AttachmentProjection.hpp"
#include "interactive/cards_remaining/MessageLeavesProjection.hpp"
#include "interactive/cards_system/SystemTextProjection.hpp"
#include "interactive/cards_gadget_result/GadgetErrorProjection.hpp"
#include "interactive/presentation_providers/ThemeState.hpp"
#include "interactive/presentation_providers/StyleRoles.hpp"
#include "interactive/gadget_views/GadgetViewProjection.hpp"
#include "terminal_ftxui/PresentationView.hpp"

namespace batbox::interactive::presentation_providers {
cards::OutputLinePalette outputLinePalette(const ThemeState&);
cards::UserCardPalette userCardPalette(const ThemeState&);
cards::PoetryPalette poetryPalette(const ThemeState&);
cards_assistant::Palette assistantPalette(const ThemeState&);
cards_system::SystemPalette systemPalette(const ThemeState&);
cards_gadget_result::GadgetErrorPalette gadgetErrorPalette(const ThemeState&);
cards_remaining::AttachmentPalette attachmentPalette(const ThemeState&);
cards_remaining::LeafPalette leafPalette(const ThemeState&,
    std::function<bool(bool)> isBlinking);
gadget_views::GadgetViewStyle gadgetViewStyle(const StyleRoles& roles,
    std::string expandKey);
terminal_ftxui::InlinePalette inlinePalette(const ThemeState&);
} // namespace batbox::interactive::presentation_providers
