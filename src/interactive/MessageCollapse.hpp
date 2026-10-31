#pragma once

#include "interactive/MessageGrouping.hpp"

namespace batbox::interactive {

RenderItemsRef collapsePartnerShutdowns(const RenderItemsRef& messages);
RenderItemsRef collapseHookSummaries(const RenderItemsRef& messages);

struct BackgroundNotificationProtocol {
  std::string missionTag;
  std::string statusTag;
  std::string summaryTag;
  std::string bashSummaryPrefix;
};
RenderItemsRef collapseBackgroundBashNotifications(
    const RenderItemsRef& messages, bool verbose, bool fullscreen,
    const BackgroundNotificationProtocol& protocol);

} // namespace batbox::interactive
