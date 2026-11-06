#include "bootstrap/CoreFlags.hpp"

namespace batbox::bootstrap {
bool getScheduledMissionsEnabled(const CoreFlags& s){
  return s.scheduledMissionsEnabled;}
void setScheduledMissionsEnabled(CoreFlags& s,bool enabled){
  s.scheduledMissionsEnabled=enabled;}
}
