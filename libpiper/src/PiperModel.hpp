#ifndef PIPER_MODEL_H
#define PIPER_MODEL_H

#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "Voice.hpp"
#include "tashkeel.hpp"
#include "wavfile.hpp"

namespace piper {

class PiperModel
{
public:
  PiperModel(const std::string& modelPath, const std::string& modelConfigPath = "");
  ~PiperModel();

  std::vector<int16_t> textToSpeech(std::string text);
  void saveToWavFile(const std::string& fileName, std::vector<int16_t> audioBuffer);

private:
  std::string eSpeakDataPath;
  bool useTashkeel = false;
  std::optional<std::string> tashkeelModelPath;
  std::unique_ptr<tashkeel::State> tashkeelState;
  Voice m_voice;
  SynthesisResult m_lastSynthesisResult;
};
} // namespace piper

#endif