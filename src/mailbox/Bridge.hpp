#pragma once
#include "mailbox/Mailbox.hpp"

namespace batbox::mailbox {
using SubmitMessage=std::function<bool(const std::string&)>;
void runMailboxBridgeEffect(Mailbox& mailbox,bool isLoading,
  const SubmitMessage& onSubmitMessage);
} // namespace batbox::mailbox
