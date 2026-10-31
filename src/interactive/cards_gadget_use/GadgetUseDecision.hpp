#pragma once

#include "interactive/MessageLookups.hpp"
#include "interactive/cards_gadget_result/HookProgressCard.hpp"
#include "gadgets/Gadget.hpp"
#include <functional>
#include <optional>

namespace batbox::interactive::cards_gadget_use {

struct TerminalSize { int columns=0,rows=0; };
struct UseRenderContext { Json theme; bool verbose=false; Json commands=Json::array(); };
struct ProgressRenderContext {
  const gadgets::Gadgets* gadgets=nullptr;
  bool verbose=false;
  TerminalSize terminalSize;
  int inProgressGadgetCallCount=1;
  bool transcriptMode=false;
};
struct UseServices {
  std::function<TerminalSize()> readTerminalSize;
  std::function<Json()> readTheme;
  std::function<Json()> readSelectedBackground;
  std::function<std::optional<std::string>()> readPendingWorkerGadgetID;
  std::function<bool(const std::string&)> readClassifierChecking;
  std::function<std::string()> readPermissionMode;
  std::function<bool()> readStrippedDangerousRules;
  std::function<std::optional<Json>(const gadgets::Gadget&,const Json&)> safeParseInput;
  std::function<std::string(const gadgets::Gadget&,const std::optional<Json>&)> userFacingName;
  std::function<Json(const gadgets::Gadget&,const std::optional<Json>&)> userFacingNameBackgroundColor;
  std::function<Json(const gadgets::Gadget&,const Json&,const UseRenderContext&)> renderGadgetUse;
  std::function<Json(const gadgets::Gadget&,const Json&)> renderTag;
  std::function<Json(const gadgets::Gadget&,const Json&,const ProgressRenderContext&)> renderProgress;
  std::function<Json(const gadgets::Gadget&)> renderQueued;
  std::function<void(const std::string&)> logError;
  std::function<int(const std::string&)> stringWidth;
};
struct GadgetUseInputs {
  Json param;
  bool addMargin=false;
  const gadgets::Gadgets* gadgets=nullptr;
  Json commands=Json::array();
  bool verbose=false;
  const GadgetIDs* inProgressGadgetUseIDs=nullptr;
  Json progressMessages=Json::array();
  bool shouldAnimate=false,shouldShowDot=false;
  std::optional<int> inProgressGadgetCallCount;
  const MessageLookups* lookups=nullptr;
  bool transcriptMode=false;
};
struct GadgetUseCard {
  enum class ProgressKind {None, WaitingPermission, GadgetProgress};
  bool visible=false, transparent=false, queued=false,resolved=false;
  bool showDot=false,animate=false,error=false;
  int nameWidth=0,marginTop=0;
  std::string name;
  Json nameBackground=Json();
  Json selectedBackground=Json();
  Json useText=Json();
  Json tag=Json();
  ProgressKind progressKind=ProgressKind::None;
  Json progress=Json();
  Json queuedMessage=Json();
  cards_gadget_result::HookProgressCard preGadgetHook;
};

GadgetUseCard classifyGadgetUse(const GadgetUseInputs& input,
                            const UseServices& services);
}
