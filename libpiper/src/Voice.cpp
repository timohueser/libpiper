#include "Voice.hpp"

#include <fstream>
#include <sstream>

using namespace piper;

Voice::Voice(const std::string& modelPath, const std::string& modelConfigPath) {
  std::string configPath = std::string(modelConfigPath);
  if (modelConfigPath == "")
  {
    configPath = std::string(modelPath) + ".json";
  }
  // Load Onnx model and JSON config file
  spdlog::debug("Parsing voice config at {}", modelConfigPath);
  std::ifstream modelConfigFile(modelConfigPath);
  configRoot = json::parse(modelConfigFile);

  parsePhonemizeConfig(configRoot, phonemizeConfig);
  parseSynthesisConfig(configRoot, synthesisConfig);

  loadModel(modelPath);
}

Voice::~Voice() {
  spdlog::debug("Destroying voice");
}

void Voice::loadModel(const std::string& modelPath) {
  spdlog::debug("Loading onnx model from {}", modelPath);
  session.env = Ort::Env(OrtLoggingLevel::ORT_LOGGING_LEVEL_WARNING, "piper");
  session.env.DisableTelemetryEvents();

  session.options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_DISABLE_ALL);

  session.options.DisableCpuMemArena();
  session.options.DisableMemPattern();
  session.options.DisableProfiling();

  auto startTime = std::chrono::steady_clock::now();

#ifdef _WIN32
  auto modelPathW = std::wstring(modelPath.begin(), modelPath.end());
  auto modelPathStr = modelPathW.c_str();
#else
  auto modelPathStr = modelPath.c_str();
#endif

  session.onnx = Ort::Session(session.env, modelPathStr, session.options);

  auto endTime = std::chrono::steady_clock::now();
  spdlog::debug("Loaded onnx model in {} second(s)", std::chrono::duration<double>(endTime - startTime).count());
}

// Load JSON config information for phonemization
void Voice::parsePhonemizeConfig(json& configRoot, PhonemizeConfig& phonemizeConfig) {
  if (configRoot.contains("espeak"))
  {
    auto espeakValue = configRoot["espeak"];
    if (espeakValue.contains("voice"))
    {
      phonemizeConfig.eSpeakVoice = espeakValue["voice"].get<std::string>();
    }
  }

  // phoneme to [id] map
  // Maps phonemes to one or more phoneme ids (required).
  if (configRoot.contains("phoneme_id_map"))
  {
    auto phonemeIdMapValue = configRoot["phoneme_id_map"];
    for (auto& fromPhonemeItem : phonemeIdMapValue.items())
    {
      std::string fromPhoneme = fromPhonemeItem.key();
      if (!isSingleCodepoint(fromPhoneme))
      {
        std::stringstream idsStr;
        for (auto& toIdValue : fromPhonemeItem.value())
        {
          PhonemeId toId = toIdValue.get<PhonemeId>();
          idsStr << toId << ",";
        }

        spdlog::error("\"{}\" is not a single codepoint (ids={})", fromPhoneme, idsStr.str());
        throw std::runtime_error("Phonemes must be one codepoint (phoneme id map)");
      }

      auto fromCodepoint = getCodepoint(fromPhoneme);
      for (auto& toIdValue : fromPhonemeItem.value())
      {
        PhonemeId toId = toIdValue.get<PhonemeId>();
        phonemizeConfig.phonemeIdMap[fromCodepoint].push_back(toId);
      }
    }
  }

  // phoneme to [phoneme] map
  // Maps phonemes to one or more other phonemes (not normally used).
  if (configRoot.contains("phoneme_map"))
  {
    if (!phonemizeConfig.phonemeMap)
    {
      phonemizeConfig.phonemeMap.emplace();
    }

    auto phonemeMapValue = configRoot["phoneme_map"];
    for (auto& fromPhonemeItem : phonemeMapValue.items())
    {
      std::string fromPhoneme = fromPhonemeItem.key();
      if (!isSingleCodepoint(fromPhoneme))
      {
        spdlog::error("\"{}\" is not a single codepoint", fromPhoneme);
        throw std::runtime_error("Phonemes must be one codepoint (phoneme map)");
      }

      auto fromCodepoint = getCodepoint(fromPhoneme);
      for (auto& toPhonemeValue : fromPhonemeItem.value())
      {
        std::string toPhoneme = toPhonemeValue.get<std::string>();
        if (!isSingleCodepoint(toPhoneme))
        {
          throw std::runtime_error("Phonemes must be one codepoint (phoneme map)");
        }

        auto toCodepoint = getCodepoint(toPhoneme);
        (*phonemizeConfig.phonemeMap)[fromCodepoint].push_back(toCodepoint);
      }
    }
  }
}

// Load JSON config for audio synthesis
void Voice::parseSynthesisConfig(json& configRoot, SynthesisConfig& synthesisConfig) {
  if (configRoot.contains("audio"))
  {
    auto audioValue = configRoot["audio"];
    if (audioValue.contains("sample_rate"))
    {
      // Default sample rate is 22050 Hz
      synthesisConfig.sampleRate = audioValue.value("sample_rate", 22050);
    }
  }

  if (configRoot.contains("inference"))
  {
    // Overrides default inference settings
    auto inferenceValue = configRoot["inference"];
    if (inferenceValue.contains("noise_scale"))
    {
      synthesisConfig.noiseScale = inferenceValue.value("noise_scale", 0.667f);
    }

    if (inferenceValue.contains("length_scale"))
    {
      synthesisConfig.lengthScale = inferenceValue.value("length_scale", 1.0f);
    }

    if (inferenceValue.contains("noise_w"))
    {
      synthesisConfig.noiseW = inferenceValue.value("noise_w", 0.8f);
    }

    if (inferenceValue.contains("phoneme_silence"))
    {
      // phoneme -> seconds of silence to add after
      synthesisConfig.phonemeSilenceSeconds.emplace();
      auto phonemeSilenceValue = inferenceValue["phoneme_silence"];
      for (auto& phonemeItem : phonemeSilenceValue.items())
      {
        std::string phonemeStr = phonemeItem.key();
        if (!isSingleCodepoint(phonemeStr))
        {
          spdlog::error("\"{}\" is not a single codepoint", phonemeStr);
          throw std::runtime_error("Phonemes must be one codepoint (phoneme silence)");
        }

        auto phoneme = getCodepoint(phonemeStr);
        (*synthesisConfig.phonemeSilenceSeconds)[phoneme] = phonemeItem.value().get<float>();
      }
    }
  }
}

// Phoneme ids to WAV audio
void Voice::synthesize(std::vector<int16_t>& audioBuffer, std::vector<PhonemeId>& phonemeIds, SynthesisResult& result) {
  spdlog::debug("Synthesizing audio for {} phoneme id(s)", phonemeIds.size());

  auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);

  // Allocate
  std::vector<int64_t> phonemeIdLengths{(int64_t) phonemeIds.size()};
  std::vector<float> scales{synthesisConfig.noiseScale, synthesisConfig.lengthScale, synthesisConfig.noiseW};

  std::vector<Ort::Value> inputTensors;
  std::vector<int64_t> phonemeIdsShape{1, (int64_t) phonemeIds.size()};
  inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(
      memoryInfo, phonemeIds.data(), phonemeIds.size(), phonemeIdsShape.data(), phonemeIdsShape.size()));

  std::vector<int64_t> phomemeIdLengthsShape{(int64_t) phonemeIdLengths.size()};
  inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(memoryInfo,
                                                           phonemeIdLengths.data(),
                                                           phonemeIdLengths.size(),
                                                           phomemeIdLengthsShape.data(),
                                                           phomemeIdLengthsShape.size()));

  std::vector<int64_t> scalesShape{(int64_t) scales.size()};
  inputTensors.push_back(Ort::Value::CreateTensor<float>(
      memoryInfo, scales.data(), scales.size(), scalesShape.data(), scalesShape.size()));

  // From export_onnx.py
  std::array<const char*, 4> inputNames = {"input", "input_lengths", "scales", "sid"};
  std::array<const char*, 1> outputNames = {"output"};

  // Infer
  auto startTime = std::chrono::steady_clock::now();
  auto outputTensors = session.onnx.Run(Ort::RunOptions{nullptr},
                                        inputNames.data(),
                                        inputTensors.data(),
                                        inputTensors.size(),
                                        outputNames.data(),
                                        outputNames.size());
  auto endTime = std::chrono::steady_clock::now();

  if ((outputTensors.size() != 1) || (!outputTensors.front().IsTensor()))
  {
    throw std::runtime_error("Invalid output tensors");
  }
  auto inferDuration = std::chrono::duration<double>(endTime - startTime);
  result.inferSeconds = inferDuration.count();

  const float* audio = outputTensors.front().GetTensorData<float>();
  auto audioShape = outputTensors.front().GetTensorTypeAndShapeInfo().GetShape();
  int64_t audioCount = audioShape[audioShape.size() - 1];

  result.audioSeconds = (double) audioCount / (double) synthesisConfig.sampleRate;
  result.realTimeFactor = 0.0;
  if (result.audioSeconds > 0)
  {
    result.realTimeFactor = result.inferSeconds / result.audioSeconds;
  }
  spdlog::debug("Synthesized {} second(s) of audio in {} second(s)", result.audioSeconds, result.inferSeconds);

  // Get max audio value for scaling
  float maxAudioValue = 0.01f;
  for (int64_t i = 0; i < audioCount; i++)
  {
    float audioValue = abs(audio[i]);
    if (audioValue > maxAudioValue)
    {
      maxAudioValue = audioValue;
    }
  }

  // We know the size up front
  audioBuffer.reserve(audioCount);

  // Scale audio to fill range and convert to int16
  float audioScale = (MAX_WAV_VALUE / std::max(0.01f, maxAudioValue));
  for (int64_t i = 0; i < audioCount; i++)
  {
    int16_t intAudioValue = static_cast<int16_t>(std::clamp(audio[i] * audioScale,
                                                            static_cast<float>(std::numeric_limits<int16_t>::min()),
                                                            static_cast<float>(std::numeric_limits<int16_t>::max())));

    audioBuffer.push_back(intAudioValue);
  }

  // Clean up
  for (std::size_t i = 0; i < outputTensors.size(); i++)
  {
    Ort::detail::OrtRelease(outputTensors[i].release());
  }

  for (std::size_t i = 0; i < inputTensors.size(); i++)
  {
    Ort::detail::OrtRelease(inputTensors[i].release());
  }
}