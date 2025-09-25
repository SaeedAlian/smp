#pragma once
#include "common/types.hpp"
#include "decoder.hpp"
#include "output.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <utility>

namespace PlayerRetCode {

enum class InitRes {
  Success = 0,
  Error,
};

enum class LoadRes {
  Success = 0,
  FailedToInitDecoder,
  DecoderNotFound,
  Error,
};

enum class PlayRes {
  Success = 0,
  FileNotLoaded,
  PlaybackIsAlreadyRunning,
  Error,
};

enum class PauseRes {
  Success = 0,
  CooldownError,
  PlaybackIsAlreadyPaused,
  PlaybackIsNotRunning,
  Error,
};

enum class ResumeRes {
  Success = 0,
  CooldownError,
  PlaybackIsNotRunning,
  PlaybackIsNotPaused,
  Error,
};

enum class StopRes {
  Success = 0,
  PlaybackIsNotRunning,
  Error,
};

enum class SeekRes {
  Success = 0,
  PlaybackIsNotRunning,
  FileNotLoaded,
  OffsetOutOfRange,
  Error,
};

}; // namespace PlayerRetCode

struct PlayerConfig {
  Enum::OutputType output_type;
  Enum::OutputDeviceType device_type;
};

constexpr std::array<std::pair<Enum::FileType, Enum::DecoderType>, 1>
    decoder_filetype_map = {{{Enum::FileType::MP3, Enum::DecoderType::MPG123}}};

constexpr Enum::DecoderType get_decoder_with_filetype(Enum::FileType type) {
  for (auto &[k, v] : decoder_filetype_map) {
    if (k == type)
      return v;
  }

  return Enum::DecoderType::UNKNOWN;
}

class Player {
public:
  Player(const PlayerConfig &config);

  PlayerRetCode::InitRes init();
  void exit();

  const uint32_t get_current_tell_sec();

  PlayerRetCode::LoadRes load(const Entity::File &file);
  PlayerRetCode::PlayRes play();
  PlayerRetCode::StopRes stop();

  PlayerRetCode::PauseRes pause();
  PlayerRetCode::ResumeRes resume();

  PlayerRetCode::SeekRes seek(int64_t offset_second);
  PlayerRetCode::SeekRes seek_to(uint32_t to_second);

  const bool is_playing();
  const bool is_paused();

private:
  PlayerConfig config;

  const Entity::File *current_file = nullptr;

  std::unique_ptr<Output> output = nullptr;
  std::unique_ptr<Decoder> decoder = nullptr;

  std::thread thrd;
  std::condition_variable cv;
  std::mutex state_mtx;

  std::atomic<bool> playback_active = false;
  std::atomic<bool> pause_action = false;
  std::atomic<bool> stop_action = false;

  std::chrono::steady_clock::time_point last_toggle_pause;
  const std::chrono::milliseconds toggle_pause_cooldown;

  void playback_loop();
};
