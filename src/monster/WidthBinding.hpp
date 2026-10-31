#pragma once
#include "monster/Sprites.hpp"
#include "text_width/StringWidth.hpp"
#include <memory>
#include <stdexcept>
namespace batbox::monster {
inline void bindGeometryWidth(GeometryServices& services,
    std::shared_ptr<text_width::StringWidth> width){
  if(!width)throw std::invalid_argument("Native terminal width provider required");
  services.stringWidth=[width=std::move(width)](const std::string& name){return width->stringWidth(name);};
}
}
