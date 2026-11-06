// Frame-time benchmark for the conversation surfaces. It builds a synthetic
// Flight through the production presentation bindings and times full-screen
// and inline frames while idle, while the spinner ticks, and while a long
// answer or long reasoning streams. With no arguments it runs a small
// configuration so the suite keeps it compiling and working; pass
// "--rows 2000 --stream-bytes 80000" for the numbers in the perf notes.
#include "application_presentation/Bindings.hpp"
#include "interactive/TranscriptRows.hpp"
#include "interactive/presentation_providers/StyleRoles.hpp"
#include "terminal_ftxui/ScrollbackProjection.hpp"
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/terminal.hpp>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace batbox;
using namespace batbox::interactive;
namespace ap=batbox::application_presentation;
namespace ip=batbox::interactive::presentation_providers;

namespace {
void check(bool condition,const std::string& reason){
  if(!condition)throw std::runtime_error(reason);
}
struct Settings {
  std::size_t rows=40,streamBytes=2000,frames=12;
  int columns=100,height=40;
};
Settings parse(int argc,char** argv){
  Settings settings;
  for(int i=1;i<argc;++i){
    const std::string name=argv[i];
    if(i+1>=argc)throw std::invalid_argument("Missing value for "+name);
    const auto number=std::stoul(argv[++i]);
    if(name=="--rows")settings.rows=number;
    else if(name=="--stream-bytes")settings.streamBytes=number;
    else if(name=="--frames")settings.frames=number;
    else if(name=="--columns")settings.columns=static_cast<int>(number);
    else if(name=="--height")settings.height=static_cast<int>(number);
    else throw std::invalid_argument("Unknown option "+name);
  }
  check(settings.rows>=2&&settings.frames>=1&&settings.columns>=40&&settings.height>=12,
    "Benchmark needs at least two rows, one frame and a 40x12 terminal");
  return settings;
}
std::string paragraph(std::size_t index){
  return "Turn "+std::to_string(index)+" keeps the lexer's `Span` offsets intact and "
    "explains **why** the parser — not the scanner — owns recovery. "
    "See [the notes](https://example.com/spans) for the edge cases.\n\n"
    "- Tokens carry their source range\n- Diagnostics point at the column\n";
}
MessageRef user(std::size_t index){
  return std::make_shared<Json>(Json{{"type","user"},{"uuid","u-"+std::to_string(index)},
    {"timestamp","2026-11-05T10:00:00.000Z"},
    {"message",{{"role","user"},{"content",Json::array({{{"type","text"},
      {"text","Question "+std::to_string(index)+": tighten the span bookkeeping in the lexer"}}})}}}});
}
MessageRef assistant(std::size_t index){
  return std::make_shared<Json>(Json{{"type","assistant"},{"uuid","a-"+std::to_string(index)},
    {"timestamp","2026-11-05T10:00:01.000Z"},
    {"message",{{"id","m-"+std::to_string(index)},{"role","assistant"},
      {"content",Json::array({{{"type","text"},{"text",paragraph(index)}}})}}}});
}
// About four bytes per token; every line ends so the inline preview shows it.
std::string streamed(std::size_t bytes,std::size_t seed){
  std::string text;
  std::size_t line=0;
  while(text.size()<bytes){
    text+=(line%9==0?"## Section ":"")+std::to_string(seed)+"."+std::to_string(line)+
      " The parser keeps its recovery points on token boundaries, so a "
      "missing brace — or a stray `;` — never costs a whole file.\n";
    if(line%9==8)text+="\n";
    ++line;
  }
  return text;
}
MessageRef streamEvent(Json event){
  return std::make_shared<Json>(Json{{"type","stream_event"},{"event",std::move(event)}});
}
MessageRef textDelta(const std::string& text){
  return streamEvent({{"type","content_block_delta"},{"index",0},
    {"delta",{{"type","text_delta"},{"text",text}}}});
}
MessageRef thinkingDelta(const std::string& text){
  return streamEvent({{"type","content_block_delta"},{"index",0},
    {"delta",{{"type","thinking_delta"},{"thinking",text}}}});
}

struct Fixture {
  event_loop::Loop loop;
  PresentationState state;
  terminal_ftxui::Options options;
  conversation_shell::Imports imports;
  ip::ThemeState theme;
  messages::Services messages;
  QueryGuard guard;
  submit::CommandQueue queue;
  std::vector<user_input::CommandEntry> catalog;
  std::shared_ptr<ap::Bundle> bundle;
  std::shared_ptr<conversation_shell::Shell> shell;
  Fixture(const Settings& settings,terminal_ftxui::Mode mode):
      imports(loop,state,(options.interactive=false,options.mode=mode,options)),
      theme({[]{return std::string("batbox-dark");},[](const std::string&){},
        []{return std::string("dark");},[]{}},"batbox-dark"),
      queue(loop){
    state.viewport.columns=settings.columns;state.viewport.rows=settings.height;
    messages.strictGadgetResultPairing=[]{return false;};
    messages.logError=[](const std::string&){};
    messages.randomUUID=[counter=std::make_shared<std::size_t>(0)]{
      return "bench-"+std::to_string(++*counter);};
    messages.nowISO=[]{return std::string("2026-11-05T10:00:00.000Z");};
    ap::Inputs inputs;
    inputs.theme=&theme;inputs.messageServices=&messages;
    inputs.shortcuts.loadBindings=[]{return std::vector<ip::Binding>{};};
    inputs.gadgets=[]{return gadgets::Gadgets{};};
    inputs.commands=[]{return Json::array();};
    inputs.modelLabel=[]{return std::string("night-owl-7b");};
    inputs.workingDirectory=[]{return std::string("/home/bat/roost");};
    inputs.homeDirectory=[]{return std::string("/home/bat");};
    inputs.permissionMode=[]{return std::string("default");};
    inputs.statusText=[]{return std::string{};};
    inputs.nowMs=[]{return 1234.0;};
    bundle=ap::Bundle::bind(imports,std::move(inputs));
    imports.submitServices=std::make_shared<submit::SubmitServices>(
      submit::SubmitServices{loop,guard,queue,catalog});
    imports.makeSubmitRequest=[](const std::string& text,const InputState&){
      submit::PoetrySubmitRequest value;value.input=text;return value;
    };
    imports.onRepin=[]{};
    imports.copyMessageText=[](const std::string&){};
    imports.editorCallbacks.invert=[](const std::string& text){return text;};
    imports.nowMs=[]{return 1234.0;};
    imports.logForDebugging=[](const std::string&){};
    imports.terminalProgressEnabled=[]{return false;};
    imports.setTerminalProgress=[](std::optional<message_list_view::ProgressState>){};
    imports.onDetachedError=[](std::exception_ptr error){
      if(error)std::rethrow_exception(error);
    };
    imports.input.onTerminalResponse=[](termio::ResponseRef){};
    imports.input.onMouse=[](keyboard_input::MouseRef){};
    imports.input.onTerminalFocus=[](bool){};
    imports.input.onSuspend=[]{};
    imports.input.onQueueBlocked=[](bool){};
    imports.input.onCommandSettled=[](const CommandDescriptor&,const CommandCompletion&,bool){};
    imports.input.onCommandError=[](const std::string& error){
      throw std::runtime_error("Command error: "+error);
    };
    imports.input.writePlain=[](std::string_view){};
    imports.input.onExit=[]{};
    imports.input.presentation.stream.nowMs=[]{return 1234.0;};
    for(std::size_t i=0;i<settings.rows/2;++i){
      state.messages.push_back(user(i));state.messages.push_back(assistant(i));
    }
    rebuildTranscriptRows(state);
    state.conversationId="bench";
    shell=conversation_shell::Shell::create(imports);
  }
};

struct Timing {double median=0,mean=0,worst=0;};
Timing measure(std::size_t frames,const std::function<void()>& frame){
  for(int i=0;i<3;++i)frame();
  std::vector<double> samples;
  for(std::size_t i=0;i<frames;++i){
    const auto start=std::chrono::steady_clock::now();
    frame();
    samples.push_back(std::chrono::duration<double,std::milli>(
      std::chrono::steady_clock::now()-start).count());
  }
  Timing timing;
  for(const auto sample:samples){timing.mean+=sample;timing.worst=std::max(timing.worst,sample);}
  timing.mean/=static_cast<double>(samples.size());
  std::sort(samples.begin(),samples.end());
  timing.median=samples[samples.size()/2];
  return timing;
}
void report(const std::string& label,const Timing& timing){
  std::cout<<std::left<<std::setw(44)<<label<<std::right<<std::fixed<<std::setprecision(3)
    <<" median "<<std::setw(9)<<timing.median<<" ms  mean "<<std::setw(9)<<timing.mean
    <<" ms  worst "<<std::setw(9)<<timing.worst<<" ms\n";
}
std::string paint(const ftxui::Element& element,const Settings& settings){
  auto screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(settings.columns),
    ftxui::Dimension::Fixed(settings.height));
  ftxui::Render(screen,element);
  return screen.ToString();
}

void fullscreen(const Settings& settings){
  Fixture fixture(settings,terminal_ftxui::Mode::Fullscreen);
  auto& shell=*fixture.shell;
  const auto frame=[&]{
    const auto text=paint(shell.renderSnapshot(),settings);
    shell.commitFrame();
    return text;
  };
  check(frame().find("Question "+std::to_string(settings.rows/2-1))!=std::string::npos,
    "Full-screen frame lacks the transcript tail");
  report("full-screen idle frame",measure(settings.frames,[&]{(void)frame();}));
  fixture.state.loading=true;fixture.state.streamMode="tool-use";
  report("full-screen spinner tick (loading)",measure(settings.frames,[&]{(void)frame();}));
  fixture.state.streamMode="responding";
  const auto body=streamed(settings.streamBytes,1);
  shell.applyStream({textDelta(body),{}});
  check(fixture.state.streamingText&&fixture.state.streamingText->size()>=settings.streamBytes,
    "Streaming text did not accumulate");
  check(frame().find("missing brace")!=std::string::npos,
    "Full-screen frame lacks the streaming answer's tail");
  std::size_t delta=0;
  report("full-screen streaming answer, per delta",measure(settings.frames,[&]{
    const auto index=delta++;
    shell.applyStream({textDelta(" token"+std::to_string(index)+(index%12==11?"\n":"")),{}});
    (void)frame();
  }));
  shell.applyStream({std::make_shared<Json>(Json{{"type","stream_request_start"}}),{}});
  fixture.state.streamingText.reset();
  fixture.state.streamMode="thinking";
  shell.applyStream({streamEvent({{"type","content_block_start"},{"index",0},
    {"content_block",{{"type","thinking"},{"thinking",""}}}}),{}});
  shell.applyStream({thinkingDelta(streamed(settings.streamBytes,2)),{}});
  check(fixture.state.streamingThinking&&
    fixture.state.streamingThinking->thinking.size()>=settings.streamBytes,
    "Streaming reasoning did not accumulate");
  report("full-screen streaming reasoning, per delta",measure(settings.frames,[&]{
    shell.applyStream({thinkingDelta(" step"+std::to_string(delta++)),{}});
    (void)frame();
  }));
}

void inlineMode(const Settings& settings){
  Fixture fixture(settings,terminal_ftxui::Mode::Inline);
  auto& state=fixture.state;
  terminal_ftxui::ScrollbackCursor cursor;
  const ftxui::Dimensions size{settings.columns,settings.height};
  // The Shell commits scrollback after every stream event and paints a frame.
  const auto event=[&](const MessageRef& value){
    applyStreamMessage(state,{value,{}},fixture.imports.input.presentation);
    (void)terminal_ftxui::advanceInlineScrollback(state,size,cursor,true);
  };
  const auto frame=[&]{
    return paint(terminal_ftxui::renderPresentationWithScrollback(state,size,cursor,true),
      settings);
  };
  (void)terminal_ftxui::advanceInlineScrollback(state,size,cursor,true);
  report("inline idle frame",measure(settings.frames,[&]{(void)frame();}));
  state.loading=true;state.streamMode="responding";
  event(textDelta(streamed(settings.streamBytes,3)));
  check(frame().find("The parser keeps")!=std::string::npos,"Inline frame lacks the preview");
  std::size_t delta=0;
  report("inline streaming answer, per delta",measure(settings.frames,[&]{
    const auto index=delta++;
    event(textDelta(" token"+std::to_string(index)+(index%12==11?"\n":"")));
    (void)frame();
  }));
}
}

int main(int argc,char** argv){
  try{
    const auto settings=parse(argc,argv);
    ftxui::Terminal::SetColorSupport(ftxui::Terminal::Color::TrueColor);
    ip::applyColorDepth(ip::ColorDepth::TrueColor);
    std::cout<<"transcript "<<settings.rows<<" messages, stream "<<settings.streamBytes
      <<" bytes, "<<settings.columns<<"x"<<settings.height<<", "<<settings.frames
      <<" timed frames\n";
    fullscreen(settings);
    inlineMode(settings);
  }catch(const std::exception& error){
    std::cerr<<error.what()<<"\n";
    return 1;
  }
}
