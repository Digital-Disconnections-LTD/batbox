#pragma once
#include "text_width/AnsiSlice.hpp"
#include "text_width/StringWidth.hpp"
#include "text_width/WrapAnsi.hpp"
#include <memory>
namespace batbox::text_width {
SliceServices indigenousSliceServices(std::shared_ptr<StringWidth> width);
}
