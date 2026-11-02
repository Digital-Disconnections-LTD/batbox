#include "terminal_ftxui/IndigenousStringMetrics.hpp"
#include "terminal_ftxui/TextFit.hpp"
#include "ftxui/screen/string.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace tf = batbox::terminal_ftxui;
namespace {
int failures=0;
void check(bool condition,const std::string& message){
  if(!condition){++failures;std::cerr<<"FAIL: "<<message<<'\n';}
}
std::string withoutSpaces(std::string text){
  std::string out;
  for(const char c:text)if(c!=' ')out.push_back(c);
  return out;
}
std::string joined(const std::vector<std::string>& rows){
  std::string out;
  for(const auto& row:rows)out+=row;
  return out;
}
// Every byte survives exactly once and no row is wider than the row width
// unless it holds a single grapheme that is.
void checkWrap(const std::string& line,int width,const std::string& name){
  std::vector<std::string> rows;
  try{rows=tf::wrapColumns(line,width);}
  catch(const std::exception& error){
    check(false,name+": threw "+error.what());return;
  }
  check(withoutSpaces(joined(rows))==withoutSpaces(batbox::value::decodeUtf8(line)),
    name+": text lost or duplicated");
  for(const auto& row:rows){
    const bool single=ftxui::Utf8ToGlyphs(row).size()<=2;
    check(tf::displayWidth(row)<=width||single,name+": row too wide: "+row);
  }
}
}

int main(){
  auto intl=std::make_shared<batbox::intl::Stash>(batbox::intl::indigenousServices());
  auto width=std::make_shared<batbox::text_width::StringWidth>(
    batbox::text_width::indigenousServices(intl));
  tf::installIndigenousStringMetrics(intl,width);

  // Width measurement throws "Invalid WTF-8" on malformed bytes; the body
  // shows them as U+FFFD instead.
  const std::string invalid="Name: \xff\xfe\xfd\xfc\xfb\xfa\xf9\xf8 then \xc3(\xe2\x82 done";
  for(int columns:{1,2,3,5,8,40})
    checkWrap(invalid,columns,"invalid UTF-8 at "+std::to_string(columns)+" columns");
  check(tf::wrapColumns("a\xff",5)==std::vector<std::string>{"a\xef\xbf\xbd"},
    "malformed byte becomes U+FFFD");
  checkWrap("ok\x01\x02\x03\x04\x05\x06\x07\x08\x0b\x0c\x0e\x0f""tail",2,"control bytes");

  // A double-width grapheme on a one-column row once looped forever.
  const std::string wide="界界界👩‍💻👩‍💻🇺🇸";
  for(int columns:{1,2,3}){
    checkWrap(wide,columns,"wide graphemes at "+std::to_string(columns)+" columns");
  }
  const auto narrow=tf::wrapColumns(wide,1);
  check(narrow==std::vector<std::string>{"界","界","界","👩‍💻","👩‍💻","🇺🇸"},
    "one grapheme per row at one column");
  check(tf::wrapColumns(wide,2).size()==6,"one wide grapheme per two-column row");
  check(tf::wrapColumns(wide,4)==std::vector<std::string>{"界界","界👩‍💻","👩‍💻🇺🇸"},
    "two wide graphemes per four-column row");

  check(tf::wrapColumns("alpha beta gamma",10)==std::vector<std::string>{"alpha beta","gamma"},
    "plain words wrap at spaces");
  check(tf::wrapColumns("abcdefghij",4)==std::vector<std::string>{"abcd","efgh","ij"},
    "long ASCII words split at the width");
  check(tf::wrapColumns("",5)==std::vector<std::string>{""},"empty line keeps one row");

  if(failures){std::cerr<<failures<<" failure(s)\n";return 1;}
  std::cout<<"wrap columns: ok\n";
  return 0;
}
