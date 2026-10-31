#include "react_scheduler/Scheduler.hpp"

namespace batbox::react_scheduler {
namespace {
double compare(const MissionRef& a,const MissionRef& b){
  const double difference=a->sortIndex-b->sortIndex;
  return difference!=0?difference:static_cast<double>(a->id)-
    static_cast<double>(b->id);
}
}
MissionRef Scheduler::peek(const std::vector<MissionRef>& heap){
  return heap.empty()?MissionRef{}:heap.front();
}
void Scheduler::push(std::vector<MissionRef>& heap,const MissionRef& node){
  std::size_t index=heap.size();heap.push_back(node);
  while(index>0){
    const auto parentIndex=(index-1)>>1;
    const auto parent=heap[parentIndex];
    if(compare(parent,node)>0){
      heap[parentIndex]=node;heap[index]=parent;index=parentIndex;
    }else break;
  }
}
MissionRef Scheduler::pop(std::vector<MissionRef>& heap){
  if(heap.empty())return {};
  const auto first=heap.front();
  const auto last=heap.back();heap.pop_back();
  if(last!=first){
    heap[0]=last;
    std::size_t index=0,length=heap.size(),halfLength=length>>1;
    while(index<halfLength){
      const auto leftIndex=2*(index+1)-1;
      const auto left=heap[leftIndex];
      const auto rightIndex=leftIndex+1;
      const auto right=heap[rightIndex];
      if(compare(left,last)<0){
        if(rightIndex<length&&compare(right,left)<0){
          heap[index]=right;heap[rightIndex]=last;index=rightIndex;
        }else{
          heap[index]=left;heap[leftIndex]=last;index=leftIndex;
        }
      }else if(rightIndex<length&&compare(right,last)<0){
        heap[index]=right;heap[rightIndex]=last;index=rightIndex;
      }else break;
    }
  }
  return first;
}
} // namespace batbox::react_scheduler
