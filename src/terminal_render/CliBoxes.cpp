#include "terminal_render/Border.hpp"

namespace batbox::terminal_render {
std::optional<BoxStyle> selectedCliBox(const std::string& name){
  if(name=="dashed")return BoxStyle{" ","╌"," ","╎"," ","╌"," ","╎"};
  if(name=="single")return BoxStyle{"┌","─","┐","│","┘","─","└","│"};
  if(name=="double")return BoxStyle{"╔","═","╗","║","╝","═","╚","║"};
  if(name=="round")return BoxStyle{"╭","─","╮","│","╯","─","╰","│"};
  if(name=="bold")return BoxStyle{"┏","━","┓","┃","┛","━","┗","┃"};
  if(name=="singleDouble")return BoxStyle{"╓","─","╖","║","╜","─","╙","║"};
  if(name=="doubleSingle")return BoxStyle{"╒","═","╕","│","╛","═","╘","│"};
  if(name=="classic")return BoxStyle{"+","-","+","|","+","-","+","|"};
  if(name=="arrow")return BoxStyle{"↘","↓","↙","←","↖","↑","↗","→"};
  return {};
}
} // namespace batbox::terminal_render
