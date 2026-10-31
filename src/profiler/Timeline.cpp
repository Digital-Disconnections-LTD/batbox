#include "profiler/Timeline.hpp"
#include "formatting/Format.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace batbox::profiler {
Timeline::Timeline(std::function<double()> now):now_(std::move(now)){
  if(!now_)throw std::invalid_argument("Profiler clock is required");
}
double Timeline::now()const{return now_();}
void Timeline::mark(const std::string& name){marks_.push_back({name,now_()});}
void Timeline::clearMarks(const std::optional<std::string>& name){
  if(!name){marks_.clear();return;}
  std::erase_if(marks_,[&](const Mark& mark){return mark.name==*name;});
}
std::string formatMs(double ms){
  std::ostringstream out;out<<std::fixed<<std::setprecision(3)<<ms;return out.str();
}
std::string formatTimelineLine(double totalMs,double deltaMs,
    const std::string& name,const std::optional<MemoryUsage>& memory,
    std::size_t totalPad,std::size_t deltaPad,const std::string& extra){
  auto pad=[](std::string value,std::size_t width){
    if(value.size()<width)value.insert(value.begin(),width-value.size(),' ');
    return value;
  };
  std::string info;
  if(memory)info=" | RSS: "+formatting::formatFileSize(memory->rss)+
    ", Heap: "+formatting::formatFileSize(memory->heapUsed);
  return "[+"+pad(formatMs(totalMs),totalPad)+"ms] (+"+
    pad(formatMs(deltaMs),deltaPad)+"ms) "+name+extra+info;
}
}
