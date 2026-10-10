#pragma once
#include <SDL3/SDL.h>

#include <functional>
#include <mutex>
#include <vector>

namespace encore {

/// Pulls stereo float audio from whichever source is current into an SDL3 audio stream.
class AudioDevice {
 public:
  using Source = std::function<void(float* out, int frames)>;

  AudioDevice() = default;
  ~AudioDevice();
  AudioDevice(const AudioDevice&) = delete;
  AudioDevice& operator=(const AudioDevice&) = delete;

  bool open(int sampleRate);
  void close();
  /// Replaces the source; safe to call while audio is playing. An empty source is silence.
  void setSource(Source source);

 private:
  static void SDLCALL callback(void* userdata, SDL_AudioStream* stream, int additionalAmount, int totalAmount);
  SDL_AudioStream* stream_ = nullptr;
  std::mutex mutex_;
  Source source_;
  std::vector<float> scratch_;
};

}  // namespace encore
