// Live answers and reasoning are drawn as pieces, one transcript row each.
// For random Markdown streamed in random fragments, stacking the pieces must
// draw exactly what the whole element draws: every cell, color and attribute.
#include "GoldenFrame.hpp"
#include "application_presentation/Bindings.hpp"
#include "interactive/presentation_providers/StyleRoles.hpp"
#include <ftxui/screen/terminal.hpp>
#include <iostream>
#include <random>
#include <stdexcept>

using namespace batbox;
using namespace batbox::interactive;
namespace ap=batbox::application_presentation;
namespace ip=batbox::interactive::presentation_providers;

namespace {
void check(bool condition,const std::string& reason){
  if(!condition)throw std::runtime_error(reason);
}
std::string paragraph(std::mt19937& random){
  static const std::vector<std::string> words{"the","parser","keeps","**spans**",
    "`Token`","*recovery*","[notes](https://example.com/n)","—","résumé","雪",
    "boundaries,","a","verylongidentifierthatwrapsacrosstherowwidthforsure","on","~~old~~",
    "[spec][ref]"};
  std::uniform_int_distribution<std::size_t> pick(0,words.size()-1),count(3,40);
  std::string text;
  for(std::size_t i=0,n=count(random);i<n;++i)text+=(i?" ":"")+words[pick(random)];
  return text;
}
std::string document(std::mt19937& random){
  std::string text;
  std::uniform_int_distribution<int> kind(0,13),gap(0,3);
  for(int block=0;block<14;++block){
    switch(kind(random)){
      case 0:{
        auto title=paragraph(random);
        auto cut=std::min<std::size_t>(30,title.size());
        while(cut<title.size()&&(static_cast<unsigned char>(title[cut])&0xc0)==0x80)++cut;
        text+="# "+title.substr(0,cut);break;
      }
      case 1:text+="## Section "+std::to_string(block);break;
      case 2:text+="- "+paragraph(random)+"\n- "+paragraph(random)+"\n  - nested item";break;
      case 3:text+="1. first\n2. "+paragraph(random)+"\n3. third";break;
      case 4:text+="- [x] done\n- [ ] open";break;
      case 5:text+="```cpp\nstruct Token { int start = 0; };\nint main() { return 0; }\n```";break;
      case 6:text+="| Stage | Time |\n|-------|-----:|\n| lex | 4 ms |\n| parse | 12 ms |";break;
      case 7:text+="---";break;
      case 8:text+="> "+paragraph(random);break;
      // Pieces cannot follow these exactly; they must still draw the same.
      case 9:if(block%5==0)text+="[ref]: https://example.com/ref";
        else text+=paragraph(random);break;
      case 10:if(block%7==0)text+="<context>notes</context> "+paragraph(random);
        else text+="Title line\n----------";break;
      default:text+=paragraph(random);break;
    }
    // Blocks may touch, or be separated by one or more blank lines.
    const auto blank=gap(random);
    text+=blank==0?"\n":blank==1?"\n\n":blank==2?"\n\n\n":"\n";
  }
  return text;
}
ftxui::Element stack(const std::vector<markdown::MarkdownPiece>& pieces){
  ftxui::Elements rows;
  for(const auto& piece:pieces){
    if(piece.gap)rows.push_back(ftxui::text(""));
    rows.push_back(piece.element);
  }
  return ftxui::vbox(std::move(rows));
}
// A screen taller than any document, so nothing is clipped; FTXUI lays the
// element out until its paragraphs settle at this width.
std::string draw(const ftxui::Element& element,int columns){
  return golden::render(element,columns,400).document();
}
}

int main(){
  try{
    ftxui::Terminal::SetColorSupport(ftxui::Terminal::Color::TrueColor);
    ip::applyColorDepth(ip::ColorDepth::TrueColor);
    event_loop::Loop loop;
    PresentationState state;
    terminal_ftxui::Options options;options.interactive=false;
    conversation_shell::Imports imports(loop,state,options);
    ip::ThemeState theme({[]{return std::string("batbox-dark");},[](const std::string&){},
      []{return std::string("dark");},[]{}},"batbox-dark");
    messages::Services messages;
    messages.strictGadgetResultPairing=[]{return false;};
    messages.logError=[](const std::string&){};
    ap::Inputs inputs;
    inputs.theme=&theme;inputs.messageServices=&messages;
    inputs.shortcuts.loadBindings=[]{return std::vector<ip::Binding>{};};
    inputs.gadgets=[]{return gadgets::Gadgets{};};inputs.commands=[]{return Json::array();};
    inputs.modelLabel=[]{return std::string("m");};
    inputs.workingDirectory=[]{return std::string("/r");};
    inputs.permissionMode=[]{return std::string("default");};
    inputs.statusText=[]{return std::string{};};
    const auto bundle=ap::Bundle::bind(imports,std::move(inputs));
    const auto projection=imports.projectionServices();
    std::mt19937 random(20261105);
    std::size_t frames=0,split=0;
    for(int round=0;round<24;++round){
      const int columns=round%3==0?60:round%3==1?80:120;
      state.viewport.columns=columns;
      const auto text=document(random);
      std::uniform_int_distribution<std::size_t> step(1,48);
      for(std::size_t at=0;at<text.size();){
        at=std::min(text.size(),at+step(random));
        while(at<text.size()&&(static_cast<unsigned char>(text[at])&0xc0)==0x80)++at;
        const auto shown=text.substr(0,at);
        const auto answer=imports.renderStreamingPieces(shown);
        check(draw(stack(answer),columns)==
          draw(imports.renderStreamingMarkdown(shown,false),columns),
          "answer pieces differ after "+std::to_string(at)+" bytes:\n"+shown);
        const auto reasoning=imports.renderThinkingPieces(shown);
        check(draw(stack(reasoning),columns)==
          draw(projection.assistant.projection.renderMarkdown(shown,true),columns),
          "reasoning pieces differ after "+std::to_string(at)+" bytes:\n"+shown);
        split+=answer.size()>2;
        ++frames;
      }
    }
    check(split>frames/2,"long answers must split into several pieces");
    // A long run of prose arriving in one burst is cut into pieces too.
    {
      state.viewport.columns=80;
      std::string prose;
      for(int i=0;i<150;++i)prose+="Paragraph "+std::to_string(i)+" **keeps** its *style* "+
        std::string(i%3?"across":"`over`")+" the cut.\n\n";
      for(const auto at:{prose.size()/3,prose.size()-7,prose.size()}){
        const auto shown=prose.substr(0,at);
        const auto answer=imports.renderStreamingPieces(shown);
        check(answer.size()>4,"a burst of prose must be cut into several pieces");
        check(draw(stack(answer),80)==draw(imports.renderStreamingMarkdown(shown,false),80),
          "burst answer pieces differ after "+std::to_string(at)+" bytes");
        check(draw(stack(imports.renderThinkingPieces(shown)),80)==
          draw(projection.assistant.projection.renderMarkdown(shown,true),80),
          "burst reasoning pieces differ after "+std::to_string(at)+" bytes");
      }
    }
    // The same text after a width change starts again.
    state.viewport.columns=72;
    const std::string again="# Title\n\nfirst\n\nsecond\n\nthird";
    check(draw(stack(imports.renderStreamingPieces(again)),72)==
      draw(imports.renderStreamingMarkdown(again,false),72),"pieces restart at a new width");
    std::cout<<frames<<" streamed frames draw identically as pieces\n";
  }catch(const std::exception& error){
    std::cerr<<error.what()<<"\n";
    return 1;
  }
}
