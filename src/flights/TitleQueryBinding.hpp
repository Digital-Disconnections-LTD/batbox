#pragma once
#include "flights/Title.hpp"
#include "model/SimpleQueries.hpp"
namespace batbox::flights {
TitleServices bindTitleQuery(event_loop::Loop& loop,TitleServices services,
  model::SimpleQueryServices query);
}
