#pragma once
#include "terminal_screen/Geometry.hpp"
#include "terminal_screen/StringPools.hpp"
#include "terminal_screen/StylePool.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::terminal_screen {
enum class CellWidth:std::int32_t {
  Narrow=0,Wide=1,SpacerTail=2,SpacerHead=3
};
struct Cell {
  std::string character=" ";
  std::int32_t styleId=0;
  CellWidth width=CellWidth::Narrow;
  std::optional<std::string> hyperlink;
};
class PackedCells {
public:
  explicit PackedCells(std::size_t cellCount):data_(cellCount,0){}
  std::size_t length64()const{return data_.size();}
  std::size_t length32()const{return data_.size()*2;}
  std::optional<std::int32_t> word(std::int64_t index)const;
  void setWord(std::int64_t index,std::int32_t value);
  void fillZero(std::size_t cellCount);
  void fillZeroRange(std::size_t start,std::size_t end);
  void copyWithin(std::size_t destination,std::size_t start,std::size_t end);
  void fillZeroJS(double start,double end);
  void copyWithinJS(double destination,double start,double end);
  void setWordsFromSubarray(const PackedCells& source,double begin,
    double end,double offset);
  std::uint64_t packed(std::size_t index)const{return data_.at(index);}
private:
  std::vector<std::uint64_t> data_;
};
struct Screen {
  double width=0,height=0;
  std::shared_ptr<PackedCells> cells;
  std::shared_ptr<std::vector<std::uint8_t>> noSelect;
  std::shared_ptr<std::vector<std::int32_t>> softWrap;
  std::shared_ptr<CharPool> charPool;
  std::shared_ptr<HyperlinkPool> hyperlinkPool;
  std::int32_t emptyStyleId=0;
  std::shared_ptr<Rectangle> damage;
};
using ScreenRef=std::shared_ptr<Screen>;
using DebugWarning=std::function<void(const std::string&,const std::string&)>;
bool isInteger(double value);
void ifNotInteger(double value,const std::string& name,
  const DebugWarning& debug);
void ifNotInteger(std::optional<double> value,const std::string& name,
  const DebugWarning& debug);
double normalizeDimension(double value);
std::int32_t packWord1(std::int32_t styleId,
  std::int32_t hyperlinkId,std::int32_t width);
ScreenRef createScreen(double width,double height,
  const std::shared_ptr<StylePool>& styles,
  const std::shared_ptr<CharPool>& charPool,
  const std::shared_ptr<HyperlinkPool>& hyperlinkPool,
  const DebugWarning& debug);
void resetScreen(Screen& screen,double width,double height,
  const DebugWarning& debug);
void migrateScreenPools(Screen& screen,
  const std::shared_ptr<CharPool>& charPool,
  const std::shared_ptr<HyperlinkPool>& hyperlinkPool);
bool isEmptyCellAt(const Screen& screen,double x,double y);
bool isCellEmpty(const Screen& screen,const Cell& cell);
std::optional<Cell> cellAt(const Screen& screen,double x,double y);
Cell cellAtIndex(const Screen& screen,double index);
void cellAtCI(const Screen& screen,std::int64_t ci,Cell& out);
std::optional<Cell> visibleCellAtIndex(const PackedCells& cells,
  const CharPool& charPool,const HyperlinkPool& hyperlinkPool,
  double index,std::int32_t lastRenderedStyleId);
std::optional<std::string> charInCellAt(const Screen& screen,double x,double y);
void setCellAt(Screen& screen,double x,double y,const Cell& cell);
void setCellStyleId(Screen& screen,double x,double y,std::int32_t styleId);
void blitRegion(Screen& dst,const Screen& src,double regionX,double regionY,
  double maxX,double maxY);
void clearRegion(Screen& screen,double regionX,double regionY,
  double regionWidth,double regionHeight);
void shiftRows(Screen& screen,double top,double bottom,double n);
void markNoSelectRegion(Screen& screen,double x,double y,
  double width,double height);
using DiffCallback=std::function<bool(double,double,const Cell*,const Cell*)>;
struct DiffEntry {
  Point point;
  std::optional<Cell> removed,added;
};
bool diffEach(const Screen& previous,const Screen& next,
  const DiffCallback& callback);
std::vector<DiffEntry> diff(const Screen& previous,const Screen& next);
std::int64_t wordOffset(double cellIndex);
} // namespace batbox::terminal_screen
