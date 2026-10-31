#include "terminal_screen/Screen.hpp"
#include "terminal_screen/TypedArraySemantics.hpp"
#include <algorithm>
#include <vector>

namespace batbox::terminal_screen {
void blitRegion(Screen& dst,const Screen& src,double regionX,double regionY,
    double maxX,double maxY){
  regionX=typed_array::jsMax(0.0,regionX);
  regionY=typed_array::jsMax(0.0,regionY);
  if(regionX>=maxX||regionY>=maxY)return;
  const auto rowLen=maxX-regionX;
  const auto srcStride=wordOffset(src.width);
  const auto dstStride=wordOffset(dst.width);
  const auto rowBytes=wordOffset(rowLen);
  typed_array::setSubarray(*dst.softWrap,*src.softWrap,regionY,maxY,regionY);
  if(regionX==0&&maxX==src.width&&src.width==dst.width){
    const auto srcStart=regionY*srcStride;
    const auto totalBytes=(maxY-regionY)*srcStride;
    dst.cells->setWordsFromSubarray(*src.cells,
      srcStart,srcStart+totalBytes,srcStart);
    const auto nsStart=regionY*src.width;
    const auto nsLen=(maxY-regionY)*src.width;
    typed_array::setSubarray(*dst.noSelect,*src.noSelect,
      nsStart,nsStart+nsLen,nsStart);
  }else{
    auto srcRowCI=regionY*srcStride+wordOffset(regionX);
    auto dstRowCI=regionY*dstStride+wordOffset(regionX);
    auto srcRowNS=regionY*src.width+regionX;
    auto dstRowNS=regionY*dst.width+regionX;
    for(auto y=regionY;y<maxY;y++){
      dst.cells->setWordsFromSubarray(*src.cells,
        srcRowCI,srcRowCI+rowBytes,dstRowCI);
      typed_array::setSubarray(*dst.noSelect,*src.noSelect,
        srcRowNS,srcRowNS+rowLen,dstRowNS);
      srcRowCI+=srcStride;dstRowCI+=dstStride;
      srcRowNS+=src.width;dstRowNS+=dst.width;
    }
  }
  const Rectangle regionRect{regionX,regionY,maxX-regionX,maxY-regionY};
  if(dst.damage)
    dst.damage=std::make_shared<Rectangle>(unionRect(*dst.damage,regionRect));
  else dst.damage=std::make_shared<Rectangle>(regionRect);
  if(maxX<dst.width){
    auto srcLastCI=wordOffset(regionY*src.width+(maxX-1));
    auto dstSpacerCI=wordOffset(regionY*dst.width+maxX);
    bool wroteSpacerOutsideRegion=false;
    for(auto y=regionY;y<maxY;y++){
      if((src.cells->word(srcLastCI+1).value_or(0)&3)==
          static_cast<std::int32_t>(CellWidth::Wide)){
        dst.cells->setWord(dstSpacerCI,1);
        dst.cells->setWord(dstSpacerCI+1,packWord1(dst.emptyStyleId,0,
          static_cast<std::int32_t>(CellWidth::SpacerTail)));
        wroteSpacerOutsideRegion=true;
      }
      srcLastCI+=srcStride;
      dstSpacerCI+=dstStride;
    }
    if(wroteSpacerOutsideRegion&&dst.damage){
      const auto rightEdge=dst.damage->x+dst.damage->width;
      if(rightEdge==maxX){
        auto expanded=*dst.damage;
        expanded.width+=1;
        dst.damage=std::make_shared<Rectangle>(expanded);
      }
    }
  }
}
} // namespace batbox::terminal_screen
