#include "indigenous_yoga/Node.hpp"

namespace batbox::indigenous_yoga {
namespace {
int generation=0,nodesVisited=0,measureCalls=0,stashHits=0,liveNodes=0;
}
Counters getYogaCounters(){
  return {nodesVisited,measureCalls,stashHits,liveNodes};
}
void incrementLiveNodes(){++liveNodes;}
void decrementLiveNodes(){--liveNodes;}
void resetCountersForLayout(){
  nodesVisited=0;measureCalls=0;stashHits=0;++generation;
}
int currentGeneration(){return generation;}
void incrementNodesVisited(){++nodesVisited;}
void incrementMeasureCalls(){++measureCalls;}
void incrementStashHits(){++stashHits;}
} // namespace batbox::indigenous_yoga
