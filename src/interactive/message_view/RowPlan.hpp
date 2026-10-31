#pragma once
#include "interactive/message_view/MessagePlan.hpp"

namespace batbox::interactive::message_view {
struct RowRequest {
  RenderItem item;
  MessageOptions options;
  std::shared_ptr<const MessageLookups> lookups;
  GadgetIDs streamingGadgetUseIDs;
  std::string screen="prompt";
  bool canAnimate=false,isLoading=false,hasContentAfter=false;
};
struct RowPlan {
  RowRequest request;
  MessageRef displayMessage;
  MessagePlan message;
  bool hasMetadata=false;
};
RowPlan prepareRow(RowRequest request,const MessageImports& imports);
bool sameRowView(const RowRequest& previous,const RowRequest& next);
MessageRefs progressFromLookup(const Json& message,const MessageLookups& lookups);
GadgetIDs siblingsFromLookup(const Json& message,const MessageLookups& lookups);
} // namespace batbox::interactive::message_view
