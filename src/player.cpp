#include "player.hpp"
#include "common/types.hpp"
#include "decoder.hpp"
#include "output.hpp"
#include <filesystem>
#include <fmt/format.h>

Player::Player(const PlayerConfig &config)
    : config(config), thrd(), playback_active(false), pause_action(false),
      stop_action(false) {}

PlayerRetCode::InitRes Player::init() {
  playback_active = false;
  pause_action = false;
  stop_action = false;

  switch (config.output_type) {
  case Enum::OutputType::ALSA: {
    output = OutputFactory::create(config.output_type);
    break;
  }

  default: {
    output = OutputFactory::create(config.output_type);
    break;
  }
  }

  OutputRetCode::InitRes res =
      output->init(output_device_str(config.device_type).c_str());

  if (res != OutputRetCode::InitRes::Success) {
    return PlayerRetCode::InitRes::Error;
  }

  return PlayerRetCode::InitRes::Success;
}

void Player::exit() {
  stop();
  if (thrd.joinable()) {
    thrd.join();
  }
  if (decoder) {
    decoder->close();
  }
  if (output) {
    output->exit();
  }
}

PlayerRetCode::LoadRes Player::load(const Entity::File &file) {
  std::lock_guard<std::mutex> lock(state_mtx);

  const std::filesystem::path fullpath =
      fmt::format("{}/{}", file.fulldir_path.c_str(), file.filename.c_str());

  const Enum::FileType filetype = file.filetype;

  Enum::DecoderType expected_decoder_type = get_decoder_with_filetype(filetype);
  if (expected_decoder_type == Enum::DecoderType::UNKNOWN) {
    return PlayerRetCode::LoadRes::DecoderNotFound;
  }

  if (decoder) {
    const Enum::DecoderType decoder_type = decoder->get_decoder_type();
    if (decoder_type != expected_decoder_type) {
      decoder = DecoderFactory::create(filetype);
    }
  } else {
    decoder = DecoderFactory::create(filetype);
  }

  if (!decoder->is_initialized()) {
    return PlayerRetCode::LoadRes::FailedToInitDecoder;
  }

  if (decoder->open(fullpath) != DecoderRetCode::OpenRes::Success)
    goto error;

  Audio::FormatInfo afi;
  if (decoder->get_format(afi) != DecoderRetCode::GetFmtRes::Success)
    goto error;
  if (decoder->set_format(afi) != DecoderRetCode::SetFmtRes::Success)
    goto error;

  if (output->open(afi) != OutputRetCode::OpenRes::Success)
    goto error;

  return PlayerRetCode::LoadRes::Success;

error:
  return PlayerRetCode::LoadRes::Error;
}

PlayerRetCode::PlayRes Player::play() {
  std::lock_guard<std::mutex> lock(state_mtx);

  if (playback_active) {
    return PlayerRetCode::PlayRes::PlaybackIsAlreadyRunning;
  }

  playback_active = true;
  pause_action = false;
  stop_action = false;

  thrd = std::thread([this]() { playback_loop(); });

  return PlayerRetCode::PlayRes::Success;
}

PlayerRetCode::PauseRes Player::pause() {
  std::lock_guard<std::mutex> lock(state_mtx);

  if (!playback_active) {
    return PlayerRetCode::PauseRes::PlaybackIsNotRunning;
  }
  pause_action = true;
  output->pause();
  return PlayerRetCode::PauseRes::Success;
}

PlayerRetCode::ResumeRes Player::resume() {
  std::lock_guard<std::mutex> lock(state_mtx);

  if (!playback_active) {
    return PlayerRetCode::ResumeRes::PlaybackIsNotRunning;
  }

  if (!pause_action) {
    return PlayerRetCode::ResumeRes::PlaybackIsNotRunning;
  }
  pause_action = false;
  output->unpause();
  cv.notify_one();

  return PlayerRetCode::ResumeRes::Success;
}

PlayerRetCode::StopRes Player::stop() {
  {
    std::lock_guard<std::mutex> lock(state_mtx);
    if (!playback_active) {
      return PlayerRetCode::StopRes::PlaybackIsNotRunning;
    }

    stop_action = true;
    pause_action = false;
    playback_active = false;

    output->stop();
  }

  cv.notify_one();

  if (thrd.joinable()) {
    thrd.join();
  }

  return PlayerRetCode::StopRes::Success;
}

PlayerRetCode::SeekRes Player::seek(double offset) {
  std::lock_guard<std::mutex> lock(state_mtx);
  if (!playback_active) {
    return PlayerRetCode::SeekRes::PlaybackIsNotRunning;
  }

  DecoderRetCode::SeekRes res = decoder->seek_cur(offset);
  if (res != DecoderRetCode::SeekRes::Success) {
    return PlayerRetCode::SeekRes::Error;
  }

  return PlayerRetCode::SeekRes::Success;
}

const bool Player::is_playing() {
  std::lock_guard<std::mutex> lock(state_mtx);
  return playback_active && !pause_action;
}

const bool Player::is_paused() {
  std::lock_guard<std::mutex> lock(state_mtx);
  return playback_active && pause_action;
}

void Player::playback_loop() {
  size_t done;
  const int bufsize = 8192;
  char *buf = new char[bufsize];

  while (true) {
    {
      std::unique_lock<std::mutex> lock(state_mtx);

      if (stop_action) {
        break;
      }

      if (pause_action) {
        cv.wait(lock, [this]() { return !pause_action || stop_action; });
        if (stop_action) {
          break;
        }
      }
    }

    if (decoder->read(buf, bufsize, done) == DecoderRetCode::ReadRes::Success) {
      output->write(buf, done);
    } else {
      break;
    }
  }

  std::lock_guard<std::mutex> lock(state_mtx);
  playback_active = false;
  delete[] buf;
}
