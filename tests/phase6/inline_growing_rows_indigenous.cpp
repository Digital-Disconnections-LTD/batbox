// The inline view wraps the streaming preview and a Gadget's partial input
// incrementally. Fed in random fragments, the lines must equal wrapping the
// whole row at once, and a cached layout must equal an uncached one.
#include "terminal_ftxui/ScrollbackProjection.hpp"
#include "interactive/TranscriptRows.hpp"
#include <iostream>
#include <random>
#include <stdexcept>

using namespace batbox;
using Kind = interactive::TranscriptRow::Kind;
namespace fx = batbox::terminal_ftxui;

namespace {
void check(bool condition,const std::string& reason){
  if(!condition)throw std::runtime_error(reason);
}
std::string describe(const std::vector<fx::VisualLine>& lines){
  std::string text;
  for(const auto& line:lines){
    text+=std::to_string(static_cast<int>(line.kind))+(line.transient?"t":"-")+
      (line.hardBreakAfter?"h":"-")+(line.selected?"s":"-")+line.id+"|";
    for(const auto& cell:line.cells)text+=cell+"\x1f";
    text+="\n";
  }
  return text;
}
std::string describe(const fx::TranscriptLineCache::Growing& growing){
  std::vector<fx::VisualLine> lines(growing.head->begin(),
    growing.head->begin()+static_cast<std::ptrdiff_t>(growing.headCount));
  if(growing.tail)lines.insert(lines.end(),growing.tail->begin(),growing.tail->end());
  return describe(lines);
}
// Words, spaces, tabs, newlines, CR LF, wide and combining characters, emoji.
const std::vector<std::string> pieces{"word ","longerword ","x","  ","\t","\n","\r\n",
  "雪","é"," ́","🦇","👩‍💻","€","a","b",
  "averyveryverylongunbrokenwordthatmustbreakacrossrows "};
std::string randomText(std::mt19937& random,std::size_t count){
  std::string text;
  std::uniform_int_distribution<std::size_t> pick(0,pieces.size()-1);
  for(std::size_t i=0;i<count;++i)text+=pieces[pick(random)];
  return text;
}
std::vector<std::size_t> cuts(std::mt19937& random,const std::string& text){
  // Fragments split anywhere a code point ends, as stream deltas can.
  std::vector<std::size_t> result;
  std::uniform_int_distribution<int> step(1,9);
  for(std::size_t at=0;at<text.size();){
    at=std::min(text.size(),at+static_cast<std::size_t>(step(random)));
    while(at<text.size()&&(static_cast<unsigned char>(text[at])&0xc0)==0x80)++at;
    result.push_back(at);
  }
  return result;
}
}

int main(){
  try{
    std::mt19937 random(20261105);
    std::size_t compared=0;
    for(int round=0;round<300;++round){
      const auto text=randomText(random,40);
      const std::size_t width=8+static_cast<std::size_t>(round%60);
      fx::TranscriptLineCache gadgetCache,previewCache;
      for(const auto at:cuts(random,text)){
        const auto shown=text.substr(0,at);
        // A Gadget row: its name, then the input received so far.
        const auto expected=describe(fx::wrapTranscriptRow(
          {Kind::Gadget,"","Write "+shown,true,{}},width));
        check(describe(gadgetCache.growing("g",Kind::Gadget,"Write ",shown,width))==expected,
          "Gadget input wrapped in fragments differs at width "+std::to_string(width));
        gadgetCache.sweep();
        // The preview shows complete lines only.
        const auto last=shown.rfind('\n');
        if(last!=std::string::npos){
          const auto visible=shown.substr(0,last+1);
          check(describe(previewCache.growing("p",Kind::Assistant,{},visible,width))==
            describe(fx::wrapTranscriptRow({Kind::Assistant,"",visible,true,{}},width)),
            "streaming preview wrapped in pieces differs at width "+std::to_string(width));
          previewCache.sweep();
        }
        ++compared;
      }
    }
    // A cached layout with commits matches the plain layout of what remains.
    interactive::PresentationState state;
    state.input.poetry="❯ ";state.loading=true;state.streamMode="responding";
    for(int i=0;i<30;++i){
      state.messages.push_back(std::make_shared<Json>(Json{{"type",i%2?"assistant":"user"},
        {"uuid","m"+std::to_string(i)},{"message",{{"role",i%2?"assistant":"user"},
          {"content",randomText(random,6)}}}}));
    }
    interactive::rebuildTranscriptRows(state);
    fx::ScrollbackCursor cursor;
    const ftxui::Dimensions size{32,12};
    std::string streamed;
    for(int step=0;step<80;++step){
      streamed+=randomText(random,1);
      state.streamingText=streamed;
      (void)fx::advanceInlineScrollback(state,size,cursor,true);
      const auto cached=fx::layoutPresentation(state,32,12,&cursor.committed,&cursor.lines);
      const auto plain=fx::layoutPresentation(state,32,12,&cursor.committed);
      check(describe(cached.visibleLines)==describe(plain.visibleLines)&&
        cached.transcriptRowCount==plain.transcriptRowCount&&cached.topRow==plain.topRow,
        "cached inline layout differs at step "+std::to_string(step));
      // Committed rows keep only their height; counts still match a full wrap.
      const auto heights=fx::layoutPresentationHeights(state,32,12,cursor.lines);
      const auto full=fx::layoutPresentation(state,32,12);
      check(heights.transcriptRowCount==full.transcriptRowCount&&
        heights.topRow==full.topRow&&heights.bodyHeight==full.bodyHeight,
        "cached heights differ at step "+std::to_string(step));
    }
    check(!cursor.committed.empty(),"rows scrolled into the terminal's scrollback");
    std::cout<<compared<<" fragment wraps and 80 cached layouts match whole wraps\n";
  }catch(const std::exception& error){
    std::cerr<<error.what()<<"\n";
    return 1;
  }
}
