#include <espeak-ng/speak_lib.h>
#include <fstream>
#include <spdlog/spdlog.h>
#include <sstream>

#include "FileManager.hpp"
#include "PiperModel.hpp"

using namespace piper;

PiperModel::PiperModel(const std::string& modelPath, const std::string& modelConfigPath)
    : m_voice(modelPath, modelConfigPath) {
  eSpeakDataPath = std::filesystem::absolute(FileManager::getDataSharePath() / "espeak-ng-data").string();

  // Enable libtashkeel for Arabic
  if (m_voice.getLanguage() == "ar")
  {
    useTashkeel = true;
    tashkeelModelPath = std::filesystem::absolute(FileManager::getDataSharePath() / "libtashkeel_model.ort").string();

    spdlog::debug("libtashkeel model is expected at {}", tashkeelModelPath.value());
  }

  // Set up espeak-ng for calling espeak_TextToPhonemesWithTerminator
  // See: https://github.com/rhasspy/espeak-ng
  spdlog::debug("Initializing eSpeak");
  int result = espeak_Initialize(AUDIO_OUTPUT_SYNCHRONOUS, 0, eSpeakDataPath.c_str(), 0);
  if (result < 0)
  {
    throw std::runtime_error("Failed to initialize eSpeak-ng"); // TODO: make error handling more
                                                                // gracefull
  }

  spdlog::debug("Initialized eSpeak");

  // Load onnx model for libtashkeel
  // https://github.com/mush42/libtashkeel/
  if (useTashkeel)
  {
    spdlog::debug("Using libtashkeel for diacritization");
    if (!tashkeelModelPath)
    {
      throw std::runtime_error("No path to libtashkeel model");
    }

    spdlog::debug("Loading libtashkeel model from {}", tashkeelModelPath.value());
    tashkeelState = std::make_unique<tashkeel::State>();
    tashkeel::tashkeel_load(tashkeelModelPath.value(), *tashkeelState);
    spdlog::debug("Initialized libtashkeel");
  }

  spdlog::info("Initialized piper");
}

PiperModel::~PiperModel() {
  // Clean up espeak-ng
  spdlog::debug("Terminating eSpeak");
  espeak_Terminate();
  spdlog::debug("Terminated eSpeak");

  spdlog::info("Terminated piper");
}

// Phonemize text and synthesize audio
std::vector<int16_t> PiperModel::textToSpeech(std::string text) {
  std::vector<int16_t> audioBuffer;
  std::size_t sentenceSilenceSamples = m_voice.getSentenceSilenceSamples();

  if (useTashkeel)
  {
    if (!tashkeelState)
    {
      throw std::runtime_error("Tashkeel model is not loaded");
    }

    spdlog::debug("Diacritizing text with libtashkeel: {}", text);
    text = tashkeel::tashkeel_run(text, *tashkeelState);
  }

  // Phonemes for each sentence
  spdlog::debug("Phonemizing text: {}", text);
  std::vector<std::vector<Phoneme>> phonemes;

  // Use espeak-ng for phonemization
  eSpeakPhonemeConfig eSpeakConfig;
  eSpeakConfig.voice = m_voice.getLanguage();
  phonemize_eSpeak(text, eSpeakConfig, phonemes);

  // Synthesize each sentence independently.
  std::vector<PhonemeId> phonemeIds;
  std::map<Phoneme, std::size_t> missingPhonemes;
  for (auto phonemesIter = phonemes.begin(); phonemesIter != phonemes.end(); ++phonemesIter)
  {
    std::vector<Phoneme>& sentencePhonemes = *phonemesIter;

    if (spdlog::should_log(spdlog::level::debug))
    {
      // DEBUG log for phonemes
      std::string phonemesStr;
      for (auto phoneme : sentencePhonemes)
      {
        utf8::append(phoneme, std::back_inserter(phonemesStr));
      }

      spdlog::debug("Converting {} phoneme(s) to ids: {}", sentencePhonemes.size(), phonemesStr);
    }

    std::vector<std::shared_ptr<std::vector<Phoneme>>> phrasePhonemes;
    std::vector<SynthesisResult> phraseResults;
    std::vector<size_t> phraseSilenceSamples;

    // Use phoneme/id map from config
    PhonemeIdConfig idConfig;
    idConfig.phonemeIdMap = std::make_shared<PhonemeIdMap>(m_voice.getPhonemeIdMap());

    if (m_voice.getPhonemeSilenceSeconds())
    {
      // Split into phrases
      std::map<Phoneme, float> phonemeSilenceSeconds = m_voice.getPhonemeSilenceSeconds().value();

      auto currentPhrasePhonemes = std::make_shared<std::vector<Phoneme>>();
      phrasePhonemes.push_back(currentPhrasePhonemes);

      for (auto sentencePhonemesIter = sentencePhonemes.begin(); sentencePhonemesIter != sentencePhonemes.end();
           sentencePhonemesIter++)
      {
        Phoneme& currentPhoneme = *sentencePhonemesIter;
        currentPhrasePhonemes->push_back(currentPhoneme);

        if (phonemeSilenceSeconds.count(currentPhoneme) > 0)
        {
          // Split at phrase boundary
          phraseSilenceSamples.push_back(
              (std::size_t)(phonemeSilenceSeconds[currentPhoneme] * m_voice.getSampleRate() * m_voice.getChannels()));

          currentPhrasePhonemes = std::make_shared<std::vector<Phoneme>>();
          phrasePhonemes.push_back(currentPhrasePhonemes);
        }
      }
    }
    else
    {
      // Use all phonemes
      phrasePhonemes.push_back(std::make_shared<std::vector<Phoneme>>(sentencePhonemes));
    }

    // Ensure results/samples are the same size
    while (phraseResults.size() < phrasePhonemes.size())
    {
      phraseResults.emplace_back();
    }

    while (phraseSilenceSamples.size() < phrasePhonemes.size())
    {
      phraseSilenceSamples.push_back(0);
    }

    // phonemes -> ids -> audio
    for (size_t phraseIdx = 0; phraseIdx < phrasePhonemes.size(); phraseIdx++)
    {
      if (phrasePhonemes[phraseIdx]->size() <= 0)
      {
        continue;
      }

      // phonemes -> ids
      phonemes_to_ids(*(phrasePhonemes[phraseIdx]), idConfig, phonemeIds, missingPhonemes);
      if (spdlog::should_log(spdlog::level::debug))
      {
        // DEBUG log for phoneme ids
        std::stringstream phonemeIdsStr;
        for (auto phonemeId : phonemeIds)
        {
          phonemeIdsStr << phonemeId << ", ";
        }

        spdlog::debug("Converted {} phoneme(s) to {} phoneme id(s): {}",
                      phrasePhonemes[phraseIdx]->size(),
                      phonemeIds.size(),
                      phonemeIdsStr.str());
      }

      // ids -> audio
      m_voice.synthesize(audioBuffer, phonemeIds, phraseResults[phraseIdx]);

      // Add end of phrase silence
      for (std::size_t i = 0; i < phraseSilenceSamples[phraseIdx]; i++)
      {
        audioBuffer.push_back(0);
      }

      m_lastSynthesisResult.audioSeconds += phraseResults[phraseIdx].audioSeconds;
      m_lastSynthesisResult.inferSeconds += phraseResults[phraseIdx].inferSeconds;

      phonemeIds.clear();
    }

    // Add end of sentence silence
    if (sentenceSilenceSamples > 0)
    {
      for (std::size_t i = 0; i < sentenceSilenceSamples; i++)
      {
        audioBuffer.push_back(0);
      }
    }

    phonemeIds.clear();
  }

  if (missingPhonemes.size() > 0)
  {
    spdlog::warn("Missing {} phoneme(s) from phoneme/id map!", missingPhonemes.size());

    for (auto phonemeCount : missingPhonemes)
    {
      std::string phonemeStr;
      utf8::append(phonemeCount.first, std::back_inserter(phonemeStr));
      spdlog::warn(
          "Missing \"{}\" (\\u{:04X}): {} time(s)", phonemeStr, (uint32_t) phonemeCount.first, phonemeCount.second);
    }
  }

  if (m_lastSynthesisResult.audioSeconds > 0)
  {
    m_lastSynthesisResult.realTimeFactor = m_lastSynthesisResult.inferSeconds / m_lastSynthesisResult.audioSeconds;
  }

  return audioBuffer;
}

// Phonemize text and synthesize audio to WAV file
void PiperModel::saveToWavFile(const std::string& fileName, std::vector<int16_t> audioBuffer) {
  // Output audio to automatically-named WAV file in a directory
  std::ofstream audioFile(fileName, std::ios::binary);

  // Write WAV
  writeWavHeader(m_voice.getSampleRate(),
                 m_voice.getSampleWidth(),
                 m_voice.getChannels(),
                 (int32_t) audioBuffer.size(),
                 audioFile);

  audioFile.write((const char*) audioBuffer.data(), sizeof(int16_t) * audioBuffer.size());
}