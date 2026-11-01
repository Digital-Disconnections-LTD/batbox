#include "interactive/text_input/Cursor.hpp"
#include "intl/IndigenousInternal.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/Unicode.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

using namespace batbox;
using batbox::interactive::text_input::Cursor;
using batbox::interactive::text_input::MetricsServices;
using Out=nlohmann::ordered_json;
static MetricsServices metrics(){
  MetricsServices services;
  services.normalizeNfc=value::normalizeNFC;
  services.grapheme=intl::indigenousSegmenter("grapheme");
  services.word=intl::indigenousSegmenter("word");
  services.stringWidth=[g=services.grapheme](const std::string& text){
    double width=0;auto cursor=g->segment(text);
    while(const auto item=cursor.next())
      width+=item->text.find("👩")!=std::string::npos?2:
        static_cast<double>(value::stringLength(item->text));
    return width;
  };
  services.wrapAnsi=[](const std::string& text,double){
    return text=="abc de"?std::string("abc\n de"):text;
  };
  return services;
}
static Out view(const Cursor& cursor){
  const auto position=cursor.getPosition();
  return Out::array({cursor.text(),cursor.offset(),position.line,
    static_cast<int>(position.column)});
}
static Out bounds(
    const std::optional<std::pair<std::size_t,std::size_t>>& value){
  return value?Out{{"start",value->first},{"end",value->second}}:Out(nullptr);
}
static Out number(const std::optional<std::size_t>& value){
  return value?Out(*value):Out(nullptr);
}
int main(){
  auto services=metrics();
  Cursor c=Cursor::fromText("a\nb",10,services,2);
  std::cout<<Out{{"name","lines"},{"values",Out::array({
    view(c),view(c.up()),view(c.down()),view(c.startOfLine()),
    view(c.endOfLine()),view(c.deleteToLineStart().cursor),
    c.deleteToLineStart().killed,view(c.endOfLogicalLine()),
    view(c.upLogicalLine()),view(c.downLogicalLine()),
    c.getViewportStartLine(1),c.getViewportCharOffset(1),
    c.getViewportCharEnd(1)})}}.dump()<<'\n';
  c=Cursor::fromText("abc def",10,services,5);
  std::cout<<Out{{"name","words"},{"values",Out::array({
    view(c),view(c.prevWord()),view(c.nextWord()),view(c.endOfWord()),
    view(c.deleteWordBefore().cursor),c.deleteWordBefore().killed,
    view(c.deleteWordAfter()),view(c.insert("Z")),view(c.del()),
    view(c.backspace()),view(c.nextVimWord()),view(c.prevVimWord()),
    view(c.nextWORD()),view(c.prevWORD()),number(c.findCharacter("f",'f',1))
  })}}.dump()<<'\n';
  c=Cursor::fromText("[Image #12] foo",20,services,11);
  auto chip=Cursor::fromText("[Image #12] foo",20,services,0);
  const auto token=chip.deleteTokenBefore();
  std::cout<<Out{{"name","chip"},{"values",Out::array({
    view(c),view(c.left()),bounds(c.referenceEndingAt(11)),
    bounds(c.referenceStartingAt(0)),c.snapOutOfReference(4,true),
    c.snapOutOfReference(4,false),view(chip.right()),
    view(token?*token:c),view(chip.backspace())
  })}}.dump()<<'\n';
  c=Cursor::fromText("👩‍💻x",10,services,5);
  std::cout<<Out{{"name","grapheme"},{"values",Out::array({
    view(c),view(c.left()),view(c.right()),view(c.backspace()),
    number(c.findCharacter("x",'f',1)),number(c.findCharacter("👩‍💻",'F',1))
  })}}.dump()<<'\n';
  c=Cursor::fromText("abc de",4,services,4);
  std::cout<<Out{{"name","wrapped"},{"values",Out::array({
    view(c),view(c.up()),view(c.down()),view(c.startOfLine()),
    view(c.endOfLine()),c.getViewportStartLine(1),
    c.getViewportCharOffset(1),c.getViewportCharEnd(1)
  })}}.dump()<<'\n';
  const auto invert=[](const std::string& text){return "<"+text+">";};
  const auto dim=[](const std::string& text){return "~"+text;};
  std::cout<<Out{{"name","render"},{"values",Out::array({
    Cursor::fromText("abc",10,services,1).render("_","",invert),
    Cursor::fromText("abc",10,services,3).render("_","",invert,
      interactive::text_input::GhostText{"界z",dim}),
    Cursor::fromText("secret123",20,services,9).render("_","*",invert),
    Cursor::fromText("abc de",4,services,4).render("_","",invert,{},1)
  })}}.dump()<<'\n';
}
