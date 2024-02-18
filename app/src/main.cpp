#include "PiperModel.hpp"
#include "Voice.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <sstream>

// TODOs:
//  - Add a way to specify the path to the voice model from the command line
//  - Add a way to set all the synthesisConfigs from the command line
//  - Add a way to set the output path from the command line
//  - Add a way to set the input text from the command line
//  - Check if loading models with different languages works fine

using namespace piper;

int main() {
  std::shared_ptr<Voice> voice = std::make_shared<Voice>("libpiper/share/voice-models/test_voice.onnx",
                                                         "libpiper/share/voice-models/test_voice.onnx.json");
  std::shared_ptr<piper::PiperModel> piperModel = std::make_shared<piper::PiperModel>(voice);

  std::vector<int16_t> audio =
      piperModel->textToSpeech("Lets do the cooking Colleen! I really want to do the COCK with you!");

  std::cout << "Audio size: " << audio.size() << std::endl;

  // Timestamp is used for path to output WAV file
  const auto now = std::chrono::system_clock::now();
  const auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

  // Generate path using timestamp
  std::filesystem::path outputPath = std::string(".");
  std::stringstream outputName;
  outputName << timestamp << ".wav";
  outputPath.append(outputName.str());

  piperModel->saveToWavFile(outputPath.string(), audio);

  return 0;
}