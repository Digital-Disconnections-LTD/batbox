#include "terminal_optimizer/Optimizer.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::terminal_optimizer {
using namespace terminal_renderer;
DiffRef optimize(const DiffRef& diff){
  if(!diff)throw std::invalid_argument("Diff required");
  if(diff->size()<=1)return diff;
  auto result=std::make_shared<Diff>();
  result->reserve(diff->size());
  std::size_t len=0;
  for(const auto& patch:*diff){
    if(const auto stdoutPatch=std::get_if<StdoutPatch>(&patch)){
      if(stdoutPatch->content.empty())continue;
    }else if(const auto move=std::get_if<CursorMovePatch>(&patch)){
      if(move->x==0&&move->y==0)continue;
    }else if(const auto clear=std::get_if<ClearPatch>(&patch)){
      if(clear->count==0)continue;
    }
    if(len>0){
      const auto lastIdx=len-1;
      const auto& last=(*result)[lastIdx];
      if(const auto move=std::get_if<CursorMovePatch>(&patch)){
        if(const auto previous=std::get_if<CursorMovePatch>(&last)){
          (*result)[lastIdx]=CursorMovePatch{previous->x+move->x,
            previous->y+move->y};
          continue;
        }
      }
      if(std::holds_alternative<CursorToPatch>(patch)&&
          std::holds_alternative<CursorToPatch>(last)){
        (*result)[lastIdx]=patch;
        continue;
      }
      if(const auto style=std::get_if<StyleStringPatch>(&patch)){
        if(const auto previous=std::get_if<StyleStringPatch>(&last)){
          (*result)[lastIdx]=StyleStringPatch{previous->str+style->str};
          continue;
        }
      }
      if(const auto link=std::get_if<HyperlinkPatch>(&patch)){
        if(const auto previous=std::get_if<HyperlinkPatch>(&last)){
          if(link->uri==previous->uri)continue;
        }
      }
      if((std::holds_alternative<CursorShowPatch>(patch)&&
          std::holds_alternative<CursorHidePatch>(last))||
          (std::holds_alternative<CursorHidePatch>(patch)&&
          std::holds_alternative<CursorShowPatch>(last))){
        result->pop_back();
        --len;
        continue;
      }
    }
    result->push_back(patch);
    ++len;
  }
  return result;
}
} // namespace batbox::terminal_optimizer
