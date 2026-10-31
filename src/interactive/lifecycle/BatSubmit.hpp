#pragma once
#include "interactive/context/GadgetUseContext.hpp"
#include "interactive/lifecycle/MessageState.hpp"
#include <future>

namespace batbox::interactive::lifecycle {
struct BatSubmitMission {
  std::string id,status;
  bool local=false;
};
struct BatSubmitHelpers {
  std::function<void(std::size_t)> setCursorOffset;
  std::function<void()> clearBuffer;
};
struct BatResumeRequest {
  std::string batId,poetry;
  std::shared_ptr<context::GadgetUseContext> gadgetUseContext;
  std::function<std::future<Json>(const Json&)> canUseGadget;
};
struct BatSubmitServices {
  event_loop::Loop& owner;
  explicit BatSubmitServices(event_loop::Loop& loop):owner(loop){}
  std::function<TurnMessageRef(const std::string&)> createUserMessage;
  std::function<void(const std::string&,TurnMessageRef)> appendMessageToLocalBat;
  std::function<void(const std::string&,const std::string&)>
    queuePendingMessage,injectUserMessageToPartner;
  std::function<std::shared_ptr<context::GadgetUseContext>(
    TurnMessagesRef,const std::vector<std::string>&,
    const std::shared_ptr<TurnAbort>&,const std::string&)>
    getGadgetUseContext;
  std::function<event_loop::Deferred<event_loop::Unit>(BatResumeRequest)>
    resumeBatBackground;
  std::function<TurnMessagesRef()> getMessages;
  std::function<std::string()> mainLoopModel;
  std::function<std::future<Json>(const Json&)> canUseGadget;
  std::function<void(std::string)> setInputValue,logForDebugging;
  std::function<void(const std::string&,const std::string&)>
    addLowPriorityNotification;
  std::function<std::string(std::exception_ptr)> errorMessage;
};
void onBatSubmit(const std::string& input,const BatSubmitMission& mission,
  const BatSubmitHelpers& helpers,std::shared_ptr<BatSubmitServices> services);
} // namespace batbox::interactive::lifecycle
