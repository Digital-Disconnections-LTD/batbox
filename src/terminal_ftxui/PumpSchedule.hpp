#pragma once
#include <algorithm>
#include <stdexcept>

namespace batbox::terminal_ftxui {
// Interval between FTXUI pumps. Input is polled inside each pump, so the
// interval stays short while anything happens and grows only after a quiet
// stretch; any input, frame, or redraw request returns it to the short value.
class PumpSchedule {
public:
  PumpSchedule(double activeMs,double idleMaxMs,int quietPumpsBeforeBackoff):
      activeMs_(activeMs),idleMaxMs_(idleMaxMs),quietPumps_(quietPumpsBeforeBackoff),
      current_(activeMs){
    if(!(activeMs_>0)||idleMaxMs_<activeMs_||quietPumps_<0)
      throw std::invalid_argument("Invalid terminal pump schedule");
  }
  // Records the outcome of one pump and returns the delay before the next.
  double next(bool active){
    if(active){wake();return current_;}
    if(quiet_<quietPumps_){++quiet_;return current_;}
    current_=std::min(idleMaxMs_,current_*2);
    return current_;
  }
  void wake(){quiet_=0;current_=activeMs_;}
  bool backedOff()const{return current_>activeMs_;}
  double current()const{return current_;}
  double activeMs()const{return activeMs_;}
private:
  double activeMs_,idleMaxMs_;
  int quietPumps_;
  int quiet_=0;
  double current_;
};
} // namespace batbox::terminal_ftxui
