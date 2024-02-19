#ifndef PHOEMIZE_H_
#define PHOEMIZE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#define CLAUSE_INTONATION_FULL_STOP 0x00000000
#define CLAUSE_INTONATION_COMMA 0x00001000
#define CLAUSE_INTONATION_QUESTION 0x00002000
#define CLAUSE_INTONATION_EXCLAMATION 0x00003000

#define CLAUSE_TYPE_CLAUSE 0x00040000
#define CLAUSE_TYPE_SENTENCE 0x00080000

#define CLAUSE_PERIOD (40 | CLAUSE_INTONATION_FULL_STOP | CLAUSE_TYPE_SENTENCE)
#define CLAUSE_COMMA (20 | CLAUSE_INTONATION_COMMA | CLAUSE_TYPE_CLAUSE)
#define CLAUSE_QUESTION (40 | CLAUSE_INTONATION_QUESTION | CLAUSE_TYPE_SENTENCE)
#define CLAUSE_EXCLAMATION (45 | CLAUSE_INTONATION_EXCLAMATION | CLAUSE_TYPE_SENTENCE)
#define CLAUSE_COLON (30 | CLAUSE_INTONATION_FULL_STOP | CLAUSE_TYPE_CLAUSE)
#define CLAUSE_SEMICOLON (30 | CLAUSE_INTONATION_COMMA | CLAUSE_TYPE_CLAUSE)

namespace piper {

typedef char32_t Phoneme;
typedef std::map<Phoneme, std::vector<Phoneme>> PhonemeMap;

struct eSpeakPhonemeConfig
{
  std::string voice = "en-us";

  Phoneme period = U'.';      // CLAUSE_PERIOD
  Phoneme comma = U',';       // CLAUSE_COMMA
  Phoneme question = U'?';    // CLAUSE_QUESTION
  Phoneme exclamation = U'!'; // CLAUSE_EXCLAMATION
  Phoneme colon = U':';       // CLAUSE_COLON
  Phoneme semicolon = U';';   // CLAUSE_SEMICOLON
  Phoneme space = U' ';

  // Remove language switch flags like "(en)"
  bool keepLanguageFlags = false;

  std::shared_ptr<PhonemeMap> phonemeMap;
};

// Phonemizes text using espeak-ng.
// Returns phonemes for each sentence as a separate std::vector.
//
// Assumes espeak_Initialize has already been called.
void phonemize_eSpeak(const std::string& text,
                      eSpeakPhonemeConfig& config,
                      std::vector<std::vector<Phoneme>>& phonemes);

void addPunctuation(std::vector<Phoneme>& sentencePhonemes, int terminator, const eSpeakPhonemeConfig& config);

} // namespace piper

#endif // PHONEMIZE_H_
