#include "indigenous_yoga/LayoutInternal.hpp"

namespace batbox::indigenous_yoga {
namespace {constexpr int STASH_SLOTS=4;}
void stashWrite(Node& node,double aW,double aH,
    MeasureMode wM,MeasureMode hM,double oW,double oH,
    bool fW,bool fH,bool wasDirty){
  if(!node._stashAllocated){
    node._cIn={};node._cOut={};node._stashAllocated=true;
  }
  if(wasDirty&&node._cGen!=currentGeneration()){
    node._cN=0;node._cWr=0;
  }
  const int index=node._cWr++%STASH_SLOTS;
  if(node._cN<STASH_SLOTS)node._cN=node._cWr;
  const int offset=index*8;
  node._cIn[offset]=aW;
  node._cIn[offset+1]=aH;
  node._cIn[offset+2]=int(wM);
  node._cIn[offset+3]=int(hM);
  node._cIn[offset+4]=oW;
  node._cIn[offset+5]=oH;
  node._cIn[offset+6]=fW?1:0;
  node._cIn[offset+7]=fH?1:0;
  node._cOut[index*2]=node.layout.width;
  node._cOut[index*2+1]=node.layout.height;
  node._cGen=currentGeneration();
}
void commitStashOutputs(Node& node,bool performLayout){
  if(performLayout){
    node._lOutW=node.layout.width;
    node._lOutH=node.layout.height;
  }else{
    node._mOutW=node.layout.width;
    node._mOutH=node.layout.height;
  }
}
} // namespace batbox::indigenous_yoga
