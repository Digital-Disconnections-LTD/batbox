#pragma once
#include "runtime/JsonValue.hpp"
#include "settings/mdm/Result.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::settings::persistence {
struct MdmValues { mdm::MdmResult mdm,hkcu; };
using CancelInterval=std::function<void()>;
struct MdmPollServices {
  std::function<mdm::MdmResult()> getMdmSettings;
  std::function<mdm::MdmResult()> getHkcuSettings;
  // Completion is delivered on the event-loop owner, including errors.
  std::function<void(std::function<void(std::optional<MdmValues>,
    std::optional<std::string>)>)> refreshMdmSettings;
  std::function<void(const MdmValues&)> setMdmSettingsStash;
  std::function<CancelInterval(double,std::function<void()>,bool)> setInterval;
  std::function<void(const std::string&)> debug;
  std::function<void()> fanOutPolicySettings;
  double pollIntervalMs=30.0*60.0*1000.0;
};

class MdmPoll {
public:
  explicit MdmPoll(MdmPollServices services);
  void start();
  void dispose();
private:
  MdmPollServices services_;
  std::optional<CancelInterval> interval_;
  std::optional<std::string> lastSnapshot_;
  bool disposed_=false;
  void tick();
};
}
