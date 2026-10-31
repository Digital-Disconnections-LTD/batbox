#include "terminal_screen/Screen.hpp"
#include "terminal_screen/TypedArraySemantics.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace batbox::terminal_screen {
bool isInteger(double value){
  return std::isfinite(value)&&std::trunc(value)==value;
}
void ifNotInteger(double value,const std::string& name,
    const DebugWarning& debug){
  if(isInteger(value))return;
  if(!debug)throw std::invalid_argument("Screen diagnostic import required");
  debug(name+" should be an integer, got "+value::numberToString(value),"warn");
}
void ifNotInteger(std::optional<double> value,const std::string& name,
    const DebugWarning& debug){
  if(!value)return;
  ifNotInteger(*value,name,debug);
}
double normalizeDimension(double value){
  if(!isInteger(value)||value<0){
    const double floored=std::floor(value);
    const double truthy=!std::isnan(floored)&&floored!=0?floored:0;
    value=std::fmax(0,truthy);
  }
  return value;
}
namespace {
std::size_t bufferCells(double size){
  const auto bits=static_cast<std::uint32_t>(typed_array::toInt32(size))<<3;
  const auto byteLength=std::bit_cast<std::int32_t>(bits);
  if(byteLength<0)throw std::range_error("Invalid array buffer length");
  return static_cast<std::size_t>(byteLength)/8;
}
std::size_t rowCount(double height){
  return typed_array::toIndex(height);
}
}
ScreenRef createScreen(double width,double height,
    const std::shared_ptr<StylePool>& styles,
    const std::shared_ptr<CharPool>& charPool,
    const std::shared_ptr<HyperlinkPool>& hyperlinkPool,
    const DebugWarning& debug){
  ifNotInteger(width,"createScreen width",debug);
  ifNotInteger(height,"createScreen height",debug);
  width=normalizeDimension(width);
  height=normalizeDimension(height);
  const auto size=width*height;
  const auto bufferSize=bufferCells(size);
  if(!styles||!charPool||!hyperlinkPool)
    throw std::invalid_argument("Screen pools required");
  auto screen=std::make_shared<Screen>();
  screen->width=width;screen->height=height;
  screen->cells=std::make_shared<PackedCells>(bufferSize);
  screen->noSelect=std::make_shared<std::vector<std::uint8_t>>(
    typed_array::toIndex(size),0);
  screen->softWrap=std::make_shared<std::vector<std::int32_t>>(
    rowCount(height),0);
  screen->charPool=charPool;screen->hyperlinkPool=hyperlinkPool;
  screen->emptyStyleId=styles->none();
  return screen;
}
void resetScreen(Screen& screen,double width,double height,
    const DebugWarning& debug){
  ifNotInteger(width,"resetScreen width",debug);
  ifNotInteger(height,"resetScreen height",debug);
  width=normalizeDimension(width);
  height=normalizeDimension(height);
  const auto size=width*height;
  if(screen.cells->length64()<size){
    screen.cells=std::make_shared<PackedCells>(bufferCells(size));
    screen.noSelect=std::make_shared<std::vector<std::uint8_t>>(
      typed_array::toIndex(size),0);
  }
  if(screen.softWrap->size()<height)
    screen.softWrap=std::make_shared<std::vector<std::int32_t>>(
      rowCount(height),0);
  screen.cells->fillZeroJS(0,size);
  typed_array::fill(*screen.noSelect,std::uint8_t{0},0,size);
  typed_array::fill(*screen.softWrap,std::int32_t{0},0,height);
  screen.width=width;screen.height=height;
  screen.damage.reset();
}
void migrateScreenPools(Screen& screen,
    const std::shared_ptr<CharPool>& charPool,
    const std::shared_ptr<HyperlinkPool>& hyperlinkPool){
  if(screen.charPool==charPool&&screen.hyperlinkPool==hyperlinkPool)return;
  if(!charPool||!hyperlinkPool)
    throw std::invalid_argument("New screen pools required");
  const auto oldChar=screen.charPool;
  const auto oldLink=screen.hyperlinkPool;
  const auto size=screen.width*screen.height;
  const auto end=wordOffset(size);
  for(std::int64_t ci=0;ci<end;ci+=2){
    const auto oldId=screen.cells->word(ci).value_or(0);
    screen.cells->setWord(ci,charPool->intern(oldChar->get(oldId)));
    const auto word1=screen.cells->word(ci+1).value_or(0);
    const auto oldLinkId=(static_cast<std::uint32_t>(word1)>>2)&0x7fff;
    if(oldLinkId!=0){
      const auto oldString=oldLink->get(oldLinkId);
      const auto newId=hyperlinkPool->intern(oldString);
      const auto styleId=static_cast<std::uint32_t>(word1)>>17;
      const auto width=word1&3;
      screen.cells->setWord(ci+1,packWord1(styleId,newId,width));
    }
  }
  screen.charPool=charPool;
  screen.hyperlinkPool=hyperlinkPool;
}
} // namespace batbox::terminal_screen
