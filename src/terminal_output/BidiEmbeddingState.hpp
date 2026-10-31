#pragma once
#include "terminal_output/Bidi.hpp"
#include "terminal_output/BidiData.generated.hpp"
#include <cstdint>
#include <map>
#include <optional>
#include <unordered_map>

namespace batbox::terminal_output {
struct BidiParagraph {int start=0,end=0,level=0;};
struct BidiLevelRun {
  int start=0,end=0,level=0;
  bool startsWithPDI=false,endsWithIsolate=false;
};
struct BidiRunSequence {
  std::vector<int> indices;
  std::uint32_t sos=0,eos=0;
};
class BidiEmbeddingState {
public:
  explicit BidiEmbeddingState(const std::string& text);
  EmbeddingLevels compute();
  std::u16string units;
  std::vector<std::uint32_t> types;
  std::vector<std::uint8_t> levels;
  std::vector<BidiParagraph> paragraphs;
  std::unordered_map<std::uint32_t,int> typeCounts;
  std::unordered_map<int,int> isolationPairs;
  void changeType(int index,std::uint32_t type);
  int count(std::uint32_t type)const;
  int autoLevel(int start,bool isFSI)const;
  int matchingPDI(int isolateStart)const;
  void resolveExplicit(const BidiParagraph& paragraph);
  std::vector<BidiRunSequence> runSequences(const BidiParagraph& paragraph);
  void resolveWeak(const BidiRunSequence& sequence);
  void resolveNeutral(const BidiRunSequence& sequence);
  void resolveImplicit(const BidiParagraph& paragraph);
};
inline constexpr std::uint32_t kIsolateInit=bidi_data::LRI|
  bidi_data::RLI|bidi_data::FSI;
inline constexpr std::uint32_t kStrong=bidi_data::L|
  bidi_data::R|bidi_data::AL;
inline constexpr std::uint32_t kNeutralIsolate=bidi_data::B|
  bidi_data::S|bidi_data::WS|bidi_data::ON|kIsolateInit|bidi_data::PDI;
inline constexpr std::uint32_t kBnLike=bidi_data::BN|
  bidi_data::RLE|bidi_data::LRE|bidi_data::RLO|
  bidi_data::LRO|bidi_data::PDF;
inline constexpr std::uint32_t kTrailing=bidi_data::S|
  bidi_data::WS|bidi_data::B|kIsolateInit|bidi_data::PDI|kBnLike;
} // namespace batbox::terminal_output
