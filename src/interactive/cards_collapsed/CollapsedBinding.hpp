#pragma once

#include "interactive/cards_collapsed/CollapsedProjection.hpp"
#include "interactive/cards_collapsed/VerboseGadgetUseProjection.hpp"

namespace batbox::interactive::cards_collapsed {

std::optional<ftxui::Element> projectCollapsedWithIndigenousGadgets(
    const CollapsedCard& card,const gadgets::Gadgets& availableGadgets,
    const MessageLookups& lookups,const GadgetIDs& inProgressGadgetUseIDs,
    const VerboseGadgetUseServices& gadgetServices,
    const VerboseProjectionImports& gadgetProjection,
    CollapsedProjectionImports imports);
}
