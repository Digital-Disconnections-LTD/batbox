#pragma once

// Golden frames for terminal views: an Element rendered at a fixed size
// becomes its text rows plus a run-length list of every styled cell
// (foreground, background and attributes). Set BATBOX_UPDATE_GOLDENS=1 to
// rewrite the files after an intentional visual change.
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace batbox::golden {

struct Frame {
  std::string text,styles;
  std::string document()const{return text+"# styles\n"+styles;}
};

inline std::string colorCode(const ftxui::Color& color,bool background){
  return color==ftxui::Color::Default?std::string("-"):color.Print(background);
}

inline std::string cellStyle(const ftxui::Cell& cell){
  std::string attributes;
  if(cell.bold)attributes+="b";
  if(cell.dim)attributes+="d";
  if(cell.italic)attributes+="i";
  if(cell.underlined)attributes+="u";
  if(cell.strikethrough)attributes+="s";
  if(cell.inverted)attributes+="v";
  const auto fg=colorCode(cell.foreground_color,false);
  const auto bg=colorCode(cell.background_color,true);
  if(fg=="-"&&bg=="-"&&attributes.empty())return {};
  return "fg="+fg+" bg="+bg+(attributes.empty()?"":" "+attributes);
}

inline Frame render(const ftxui::Element& element,int width,int height){
  auto screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(width),
    ftxui::Dimension::Fixed(height));
  ftxui::Render(screen,element);
  Frame frame;
  std::ostringstream styles;
  for(int y=0;y<height;++y){
    std::string row;
    int skip=0;
    for(int x=0;x<width;++x){
      const auto& cell=screen.CellAt(x,y);
      if(skip>0){--skip;continue;}
      const auto glyph=cell.character.empty()?std::string(" "):cell.character;
      row+=glyph;
      if(ftxui::string_width(glyph)==2)skip=1;
    }
    while(!row.empty()&&row.back()==' ')row.pop_back();
    frame.text+=row+"\n";
    // Runs of identical non-default style on this row.
    int start=0;
    std::string current=cellStyle(screen.CellAt(0,y));
    for(int x=1;x<=width;++x){
      const auto next=x<width?cellStyle(screen.CellAt(x,y)):std::string("\x01");
      if(next==current)continue;
      if(!current.empty())styles<<y<<":"<<start<<"-"<<(x-1)<<" "<<current<<"\n";
      start=x;current=next;
    }
  }
  frame.styles=styles.str();
  return frame;
}

// Returns true when the golden matches (or was rewritten on request).
inline bool compare(const std::filesystem::path& directory,const std::string& name,
    const Frame& frame){
  const auto path=directory/(name+".txt");
  const auto actual=frame.document();
  const char* update=std::getenv("BATBOX_UPDATE_GOLDENS");
  if(update&&std::string(update)=="1"){
    std::filesystem::create_directories(directory);
    std::ofstream(path,std::ios::binary)<<actual;
    return true;
  }
  std::ifstream input(path,std::ios::binary);
  if(!input){
    std::cerr<<"Missing golden "<<path<<" (run with BATBOX_UPDATE_GOLDENS=1)\n";
    return false;
  }
  std::stringstream expected;expected<<input.rdbuf();
  if(expected.str()==actual)return true;
  const auto lines=[](const std::string& text){
    std::vector<std::string> result;std::istringstream stream(text);std::string line;
    while(std::getline(stream,line))result.push_back(line);
    return result;
  };
  const auto want=lines(expected.str()),got=lines(actual);
  std::cerr<<"Golden mismatch: "<<path<<"\n";
  int shown=0;
  for(std::size_t i=0;i<std::max(want.size(),got.size())&&shown<12;++i){
    const auto a=i<want.size()?want[i]:"<end>",b=i<got.size()?got[i]:"<end>";
    if(a==b)continue;
    std::cerr<<"  line "<<i+1<<"\n    expected: "<<a<<"\n    actual:   "<<b<<"\n";
    ++shown;
  }
  return false;
}

} // namespace batbox::golden
