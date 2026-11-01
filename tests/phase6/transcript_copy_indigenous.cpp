#include "terminal_ftxui/TranscriptCopy.hpp"
#include <stdexcept>
#include <iostream>

namespace ix=batbox::interactive;
namespace tf=batbox::terminal_ftxui;

int main(){
  ix::PresentationState state;
  state.transcriptRows={
    {ix::TranscriptRow::Kind::User,"a","alpha beta",false,"a"},
    {ix::TranscriptRow::Kind::Assistant,"b","gamma delta",false,"b"},
    {ix::TranscriptRow::Kind::Assistant,"c","epsilon zeta",false,"c"}
  };
  tf::TranscriptCopy copy(state,20,4);
  copy.scrollBy(-100);
  if(copy.top()!=0)throw std::runtime_error("Copy view failed to reach transcript start");
  copy.press(6,1);
  copy.move(6,2);
  copy.scrollBy(1);
  if(copy.top()!=1||copy.selectedText()!="beta\ngamma delta\nepsilon")
    throw std::runtime_error("Selection did not span scrolled transcript rows: "+
      copy.selectedText());
  if(copy.release(6,2)!=std::optional<std::string>{
      "beta\ngamma delta\nepsilon"})
    throw std::runtime_error("Drag release did not copy the complete range");
  state.transcriptRows={
    {ix::TranscriptRow::Kind::User,"d","ab\ncd",false,"d"}
  };
  tf::TranscriptCopy newline(state,2,5);
  newline.press(0,1);newline.move(1,2);
  if(newline.selectedText()!="ab\ncd")
    throw std::runtime_error("Explicit line break was not preserved");
  state.transcriptRows={
    {ix::TranscriptRow::Kind::Assistant,"e","abcdefghij",false,"e"}
  };
  tf::TranscriptCopy wrapped(state,5,5);
  wrapped.press(2,1);wrapped.move(2,2);
  if(wrapped.selectedText()!="cdefgh")
    throw std::runtime_error("Soft wrap inserted a line break");
  state.transcriptRows={
    {ix::TranscriptRow::Kind::Assistant,"f","A界B",false,"f"}
  };
  tf::TranscriptCopy wide(state,6,5);
  wide.press(2,1);wide.move(3,1);
  if(wide.selectedText()!="界B")
    throw std::runtime_error("Selection starting on a wide glyph lost text");
  std::cout<<"Transcript copy selection spans scroll and preserves breaks\n";
}
