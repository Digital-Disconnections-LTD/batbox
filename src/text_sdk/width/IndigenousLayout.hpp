#pragma once
#include "text_sdk/width/AnsiSlice.hpp"
#include "text_sdk/width/StringWidth.hpp"
#include "text_sdk/width/WrapAnsi.hpp"
#include <memory>
namespace batbox::text_sdk::width {
SliceServices indigenousSliceServices(std::shared_ptr<StringWidth> width);
}
