#include "intl/Intl.hpp"
namespace batbox::intl {
Stash::Stash(Services services):services_(std::move(services)){}
std::shared_ptr<Segmenter> Stash::getGraphemeSegmenter(){
  if(!grapheme_)grapheme_=services_.createSegmenter("grapheme");
  return grapheme_;
}
std::shared_ptr<Segmenter> Stash::getWordSegmenter(){
  if(!word_)word_=services_.createSegmenter("word");
  return word_;
}
} // namespace batbox::intl
