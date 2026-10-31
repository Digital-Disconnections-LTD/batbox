#include "terminal_screen/Screen.hpp"
#include "terminal_screen/TypedArraySemantics.hpp"
#include <cmath>
#include <limits>

namespace batbox::terminal_screen {
namespace {
std::int32_t wordOrZero(const PackedCells& cells,std::int64_t index){
  return cells.word(index).value_or(0);
}
bool isEmptyCellByIndex(const Screen& screen,double index){
  const auto ci=wordOffset(index);
  const auto first=screen.cells->word(ci),second=screen.cells->word(ci|1);
  return first&&second&&*first==0&&*second==0;
}
Cell readCell(const PackedCells& cells,const CharPool& chars,
    const HyperlinkPool& links,std::int64_t ci){
  const auto first=cells.word(ci);
  const auto word1=wordOrZero(cells,ci+1);
  const auto bits=static_cast<std::uint32_t>(word1);
  const auto hid=(bits>>2)&0x7fff;
  return {chars.get(first?static_cast<double>(*first):
      std::numeric_limits<double>::quiet_NaN()),
    static_cast<std::int32_t>(bits>>17),
    static_cast<CellWidth>(bits&3),
    hid==0?std::optional<std::string>{}:links.get(hid)};
}
}
bool isEmptyCellAt(const Screen& screen,double x,double y){
  if(x<0||y<0||x>=screen.width||y>=screen.height)return true;
  return isEmptyCellByIndex(screen,y*screen.width+x);
}
bool isCellEmpty(const Screen& screen,const Cell& cell){
  return cell.character==" "&&cell.styleId==screen.emptyStyleId&&
    cell.width==CellWidth::Narrow&&
    (!cell.hyperlink||cell.hyperlink->empty());
}
std::optional<Cell> cellAt(const Screen& screen,double x,double y){
  if(x<0||y<0||x>=screen.width||y>=screen.height)return {};
  return cellAtIndex(screen,y*screen.width+x);
}
Cell cellAtIndex(const Screen& screen,double index){
  return readCell(*screen.cells,*screen.charPool,*screen.hyperlinkPool,
    wordOffset(index));
}
void cellAtCI(const Screen& screen,std::int64_t ci,Cell& out){
  const auto word1=wordOrZero(*screen.cells,
    typed_array::toInt32(static_cast<double>(ci))|1);
  const auto bits=static_cast<std::uint32_t>(word1);
  const auto characterId=screen.cells->word(ci);
  out.character=screen.charPool->get(characterId?
    static_cast<double>(*characterId):
    std::numeric_limits<double>::quiet_NaN());
  out.styleId=static_cast<std::int32_t>(bits>>17);
  out.width=static_cast<CellWidth>(bits&3);
  const auto hid=(bits>>2)&0x7fff;
  out.hyperlink=hid==0?std::optional<std::string>{}:
    screen.hyperlinkPool->get(hid);
}
std::optional<Cell> visibleCellAtIndex(const PackedCells& cells,
    const CharPool& charPool,const HyperlinkPool& hyperlinkPool,
    double index,std::int32_t lastRenderedStyleId){
  const auto ci=wordOffset(index);
  const auto charId=cells.word(ci);
  if(charId&&*charId==1)return {};
  const auto word1=wordOrZero(cells,ci+1);
  const auto bits=static_cast<std::uint32_t>(word1);
  if(charId&&*charId==0&&(bits&0x3fffc)==0){
    const auto fgStyle=static_cast<std::int32_t>(bits>>17);
    if(fgStyle==0||fgStyle==lastRenderedStyleId)return {};
  }
  return readCell(cells,charPool,hyperlinkPool,ci);
}
std::optional<std::string> charInCellAt(const Screen& screen,
    double x,double y){
  if(x<0||y<0||x>=screen.width||y>=screen.height)return {};
  const auto ci=wordOffset(y*screen.width+x);
  const auto first=screen.cells->word(ci);
  return screen.charPool->get(first?static_cast<double>(*first):
    std::numeric_limits<double>::quiet_NaN());
}
} // namespace batbox::terminal_screen
