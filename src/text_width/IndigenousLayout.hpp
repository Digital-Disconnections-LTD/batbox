#pragma once
#include "text_width/AnsiSlice.hpp"
#include "text_width/WrapAnsi.hpp"
namespace batbox::text_width {
SliceServices indigenousSliceServices(std::shared_ptr<StringWidth> width);
WrapAnsiModule indigenousWrapAnsiModule(bool bunAvailable=true);
WrapServices indigenousLayoutServices(std::shared_ptr<StringWidth> width,WrapAnsiRef wrapAnsi);
}
