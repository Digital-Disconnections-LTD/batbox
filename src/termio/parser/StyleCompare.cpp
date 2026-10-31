#include "termio/parser/StyleCompare.hpp"

namespace batbox::termio::parser {
bool colorsEqual(const Color& a,const Color& b){
  if(a.type!=b.type)return false;
  if(a.type=="named")return a.name==b.name;
  if(a.type=="indexed")return a.index==b.index;
  if(a.type=="rgb")return a.r==b.r&&a.g==b.g&&a.b==b.b;
  return a.type=="default";
}
bool stylesEqual(const TextStyle& a,const TextStyle& b){
  return a.bold==b.bold&&a.dim==b.dim&&a.italic==b.italic&&
    a.underline==b.underline&&a.blink==b.blink&&a.inverse==b.inverse&&
    a.hidden==b.hidden&&a.strikethrough==b.strikethrough&&
    a.overline==b.overline&&colorsEqual(a.fg,b.fg)&&
    colorsEqual(a.bg,b.bg)&&colorsEqual(a.underlineColor,b.underlineColor);
}
}
