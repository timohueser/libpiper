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
  PiperModel(std::shared_ptr<Voice> voice);
  ~PiperModel();

  std::vector<int16_t> textToSpeech(std::string text);
  void saveToWavFile(std::string fileName, std::vector<int16_t> audioBuffer);

private:
  std::string eSpeakDataPath;
  bool useTashkeel = false;
  std::optional<std::string> tashkeelModelPath;
  std::unique_ptr<tashkeel::State> tashkeelState;
  std::shared_ptr<Voice> m_voice;
  SynthesisResult m_lastSynthesisResult;
};
} // namespace piper

#endif