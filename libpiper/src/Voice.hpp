#ifndef VOICE_H
#define VOICE_H

#include <map>
#include <onnxruntime_cxx_api.h>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

#include "json.hpp"
#include "phoneme_ids.hpp"
#include "phonemize.hpp"
#include "utf8.h"

using json = nlohmann::json;

namespace piper {

struct PhonemizeConfig
{
  std::optional<std::map<Phoneme, std::vector<Phoneme>>> phonemeMap;
  std::map<Phoneme, std::vector<PhonemeId>> phonemeIdMap;

  PhonemeId idPad = 0; // padding (optionally interspersed)
  PhonemeId idBos = 1; // beginning of sentence
  PhonemeId idEos = 2; // end of sentence
  bool interspersePad = true;

  std::string eSpeakVoice = "en-us";
};

struct SynthesisConfig
{
  // VITS inference settings
  float noiseScale = 0.667f;
  float lengthScale = 1.0f;
  float noiseW = 0.8f;

  // Audio settings
  int sampleRate = 22050;
  int sampleWidth = 2; // 16-bit
  int channels = 1;    // mono

  // Extra silence
  float sentenceSilenceSeconds = 0.2f;
  std::optional<std::map<piper::Phoneme, float>> phonemeSilenceSeconds;
};

struct SynthesisResult
{
  double inferSeconds;
  double audioSeconds;
  double realTimeFactor;
};

struct ModelSession
{
  Ort::Session onnx;
  Ort::AllocatorWithDefaultOptions allocator;
  Ort::SessionOptions options;
  Ort::Env env;

  ModelSession() : onnx(nullptr){};
};

class Voice
{
public:
  Voice(const std::string& modelPath, const std::string& modelConfigPath);
  ~Voice();

  void synthesize(std::vector<int16_t>& audioBuffer, std::vector<PhonemeId>& phonemeIds, SynthesisResult& result);

  std::string getLanguage() { return phonemizeConfig.eSpeakVoice; }
  std::size_t getSentenceSilenceSamples() {
    return synthesisConfig.sampleRate * synthesisConfig.sentenceSilenceSeconds;
  }
  std::map<Phoneme, std::vector<PhonemeId>> getPhonemeIdMap() { return phonemizeConfig.phonemeIdMap; }
  std::optional<std::map<piper::Phoneme, float>> getPhonemeSilenceSeconds() {
    return synthesisConfig.phonemeSilenceSeconds;
  }
  int getSampleRate() { return synthesisConfig.sampleRate; }
  int getSampleWidth() { return synthesisConfig.sampleWidth; }
  int getChannels() { return synthesisConfig.channels; }

private:
  json configRoot;
  PhonemizeConfig phonemizeConfig;
  SynthesisConfig synthesisConfig;
  ModelSession session;
  const float MAX_WAV_VALUE = 32767.0f;

  void loadModel(const std::string& modelPath);
  void parsePhonemizeConfig(json& configRoot, PhonemizeConfig& phonemizeConfig);
  void parseSynthesisConfig(json& configRoot, SynthesisConfig& synthesisConfig);

  static bool isSingleCodepoint(std::string s) { return utf8::distance(s.begin(), s.end()) == 1; }

  static Phoneme getCodepoint(std::string s) {
    utf8::iterator character_iter(s.begin(), s.begin(), s.end());
    return *character_iter;
  }
};
} // namespace piper

#endif