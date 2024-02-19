#include <map>
#include <string>
#include <vector>

#include <espeak-ng/speak_lib.h>

#include "phonemize.hpp"
#include "uni_algo.h"

namespace piper {

// language -> phoneme -> [phoneme, ...]
std::map<std::string, PhonemeMap> DEFAULT_PHONEME_MAP = {{"pt-br", {{U'c', {U'k'}}}}};

void phonemize_eSpeak(const std::string& text,
                      eSpeakPhonemeConfig& config,
                      std::vector<std::vector<Phoneme>>& phonemes) {

  if (espeak_SetVoiceByName(config.voice.c_str()) != EE_OK)
  {
    throw std::runtime_error("Failed to set eSpeak-ng voice");
  }
  std::shared_ptr<PhonemeMap> phonemeMap;

  if (config.phonemeMap)
  {
    phonemeMap = config.phonemeMap;
  }
  if (DEFAULT_PHONEME_MAP.find(config.voice) != DEFAULT_PHONEME_MAP.end())
  {
    phonemeMap = std::make_shared<PhonemeMap>(DEFAULT_PHONEME_MAP[config.voice]);
  }

  std::string textCopy = text;

  // std::vector<Phoneme>* sentencePhonemes = nullptr;
  const char* inputTextPointer = textCopy.c_str();
  int terminator = 0;

  while (inputTextPointer)
  {
    std::string clausePhonemes =
        espeak_TextToPhonemesWithTerminator((const void**) &inputTextPointer, espeakCHARS_AUTO, 0x02, &terminator);
    auto phonemesNorm = una::norm::to_nfd_utf8(clausePhonemes);
    std::vector<Phoneme> sentencePhonemes;

    for (const auto& phoneme : una::ranges::utf8_view{phonemesNorm})
    {
      auto it = phonemeMap && phonemeMap->find(phoneme) != phonemeMap->end() ? phonemeMap->at(phoneme)
                                                                             : std::vector<Phoneme>{phoneme};
      if (!config.keepLanguageFlags || (phoneme != U'(' && phoneme != U')'))
      {
        sentencePhonemes.insert(end(sentencePhonemes), begin(it), end(it));
      }
    }

    addPunctuation(sentencePhonemes, terminator, config);

    phonemes.push_back(std::move(sentencePhonemes));
  }
}

void addPunctuation(std::vector<Phoneme>& sentencePhonemes, int terminator, const eSpeakPhonemeConfig& config) {
  // This function adds punctuation based on the terminator type
  switch (terminator & 0x000FFFFF)
  {
  case CLAUSE_PERIOD:
    sentencePhonemes.push_back(config.period);
    break;
  case CLAUSE_QUESTION:
    sentencePhonemes.push_back(config.question);
    break;
  case CLAUSE_EXCLAMATION:
    sentencePhonemes.push_back(config.exclamation);
    break;
  case CLAUSE_COMMA:
    sentencePhonemes.push_back(config.comma);
    sentencePhonemes.push_back(config.space);
    break;
    // Add cases for other punctuation as needed
  }
}

} // namespace piper