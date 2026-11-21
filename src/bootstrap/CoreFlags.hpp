#pragma once

namespace batbox::bootstrap {
struct CoreFlags {
  bool scheduledMissionsEnabled=false;
};
inline bool getScheduledMissionsEnabled(const CoreFlags& state){
  return state.scheduledMissionsEnabled;}
inline void setScheduledMissionsEnabled(CoreFlags& state,bool enabled){
  state.scheduledMissionsEnabled=enabled;}
}
