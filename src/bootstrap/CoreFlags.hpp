#pragma once

namespace batbox::bootstrap {
struct CoreFlags {
  bool scheduledMissionsEnabled=false;
};
bool getScheduledMissionsEnabled(const CoreFlags& state);
void setScheduledMissionsEnabled(CoreFlags& state,bool enabled);
}
