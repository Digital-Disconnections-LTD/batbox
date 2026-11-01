target_sources(batbox_interactive PRIVATE
  src/interactive/cards/BashResultCard.cpp
  src/interactive/cards/BashResultText.cpp
  src/interactive/cards/OutputLineCard.cpp
  src/interactive/cards/OutputLineText.cpp
  src/interactive/cards/OutputLineTruncation.cpp
  src/interactive/cards/ThinkingHighlight.cpp
  src/interactive/cards/UserPoetryLeaf.cpp
  src/interactive/cards/UserTextCard.cpp
  src/interactive/cards/UserTextLeaves.cpp)
target_link_libraries(batbox_interactive PUBLIC batbox_ansi_tokenize batbox_intl)
target_sources(batbox_terminal_ftxui PRIVATE
  src/interactive/cards/BashResultProjection.cpp
  src/interactive/cards/OutputLineProjection.cpp
  src/interactive/cards/UserBashOutputProjection.cpp
  src/interactive/cards/UserPoetryProjection.cpp
  src/interactive/cards/UserTextBinding.cpp
  src/interactive/cards/UserTextProjection.cpp)
target_sources(batbox_interactive PRIVATE
  src/interactive/cards_assistant/AssistantTextCard.cpp
  src/interactive/cards_assistant/ThinkingCard.cpp
  src/interactive/cards_assistant/RateLimitCard.cpp
  src/interactive/cards_system/SystemAPIErrorBinding.cpp
  src/interactive/cards_system/SystemAPIErrorCard.cpp
  src/interactive/cards_system/SystemAPIErrorTicker.cpp
  src/interactive/cards_system/SystemTextRouter.cpp)
target_sources(batbox_terminal_ftxui PRIVATE
  src/interactive/cards_assistant/AssistantProjection.cpp
  src/interactive/cards_assistant/AssistantBinding.cpp
  src/interactive/cards_system/SystemAPIErrorProjection.cpp
  src/interactive/cards_system/SystemTextProjection.cpp)
target_sources(batbox_interactive PRIVATE
  src/interactive/cards_gadget_result/FallbackErrorCard.cpp
  src/interactive/cards_gadget_result/HookProgressCard.cpp
  src/interactive/cards_gadget_result/GadgetErrorCard.cpp
  src/interactive/cards_gadget_result/GadgetResultRouter.cpp
  src/interactive/cards_gadget_children/GadgetSuccessCard.cpp
  src/interactive/cards_gadget_children/GadgetRejectCard.cpp)
target_sources(batbox_terminal_ftxui PRIVATE
  src/interactive/cards_gadget_result/HookProgressProjection.cpp
  src/interactive/cards_gadget_result/GadgetErrorProjection.cpp
  src/interactive/cards_gadget_result/GadgetResultProjection.cpp
  src/interactive/cards_gadget_children/GadgetSuccessProjection.cpp
  src/interactive/cards_gadget_children/GadgetRejectProjection.cpp)
target_sources(batbox_interactive PRIVATE
  src/interactive/cards_gadget_use/GadgetUseDecision.cpp
  src/interactive/cards_gadget_use/GroupedGadgetUse.cpp)
target_sources(batbox_terminal_ftxui PRIVATE
  src/interactive/cards_gadget_use/GadgetUseProjection.cpp)
target_sources(batbox_interactive PRIVATE
  src/interactive/cards_collapsed/CollapsedPlan.cpp
  src/interactive/cards_collapsed/CollapsedSummary.cpp
  src/interactive/cards_collapsed/VerboseGadgetUse.cpp
  src/interactive/cards_collapsed/MinHintDisplay.cpp)
target_sources(batbox_terminal_ftxui PRIVATE
  src/interactive/cards_collapsed/CollapsedProjection.cpp
  src/interactive/cards_collapsed/VerboseGadgetUseProjection.cpp
  src/interactive/cards_collapsed/CollapsedBinding.cpp
  src/interactive/cards_gadget_binding/GadgetChildBinding.cpp
  src/interactive/message_view/RetainedViews.cpp)
target_sources(batbox_interactive PRIVATE
  src/interactive/cards_remaining/AttachmentCard.cpp
  src/interactive/cards_remaining/AttachmentMailbox.cpp
  src/interactive/cards_remaining/AttachmentMissionStatus.cpp
  src/interactive/cards_remaining/MessageLeaves.cpp)
target_sources(batbox_terminal_ftxui PRIVATE
  src/interactive/cards_remaining/AttachmentProjection.cpp
  src/interactive/cards_remaining/MessageLeavesProjection.cpp)
target_sources(batbox_interactive PRIVATE
  src/interactive/gadget_views/GadgetViewModel.cpp)
target_sources(batbox_terminal_ftxui PRIVATE
  src/terminal_ftxui/TextFit.cpp
  src/interactive/gadget_views/GadgetViewProjection.cpp)
