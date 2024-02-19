#include "Piper.hpp"

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
  PiperModel piperModel("/Users/timo/Documents/piper-voices/en/en_US/kusal/medium/en_US-kusal-medium.onnx",
                        "/Users/timo/Documents/piper-voices/en/en_US/kusal/medium/en_US-kusal-medium.onnx.json");

  const char* story =
      "JARVIS makes highly precise markerless 3D motion capture easy. All you need to get started is a multi camera "
      "recording setup, and an idea of what you want to track. Our Toolbox will assist you on every step along the "
      "way, "
      "from recording synchronised videos, to quickly and consistently annotating your data, all the way to the final "
      "3D pose predictions. If you are interested in setting up a 3D Motion Capture System or just want to learn more "
      "about our toolbox, we strongly recommend having a look at our Getting Started Guide and our Manual. Here you'll "
      "find an overview of our workflow, as well as tutorials to help you build a successful 3D tracking pipeline with "
      "JARVIS.";

  std::vector<int16_t> audio = piperModel.textToSpeech(story);

  // Timestamp is used for path to output WAV file
  const auto now = std::chrono::system_clock::now();
  const auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

  // Generate path using timestamp
  std::filesystem::path outputPath = std::string(".");
  std::stringstream outputName;
  outputName << timestamp << ".wav";
  outputPath.append(outputName.str());

  piperModel.saveToWavFile(outputPath.string(), audio);

  return 0;
}