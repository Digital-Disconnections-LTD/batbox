#pragma once
#include "flights/Title.hpp"
#include "settings/persistence/SafeJson.hpp"
#include <memory>
namespace batbox::flights {
TitleServices bindTitleParser(TitleServices services,
  std::shared_ptr<settings::persistence::SafeJsonParser> parser);
}
