#pragma once
#include "common/types.hpp"
#include <alsa/asoundlib.h>

namespace OutputRetCode {

enum class InitRes {
  Success = 0,
  Error,
};

enum class ExitRes {
  Success = 0,
};

enum class OpenRes {
  Success = 0,
  OpenError,
  SetParamsError,
  PrepareError,
};

enum class CloseRes {
  Success = 0,
  DrainError,
  CloseError,
};

enum class UnlockRes {
  Success = 0,
};

enum class LockRes {
  Success = 0,
};

enum class WriteRes {
  Success = 0,
  Error,
};

enum class StopRes {
  Success = 0,
  Error,
};

enum class PauseRes {
  Success = 0,
  Dropped,
  Error,
  InvalidState,
};

enum class UnpauseRes {
  Success = 0,
  Prepared,
  Error,
  InvalidState,
};

} // namespace OutputRetCode

class Output {
public:
  virtual OutputRetCode::InitRes init(const char *device) = 0;
  virtual OutputRetCode::ExitRes exit() = 0;

  virtual OutputRetCode::OpenRes open(const Audio::FormatInfo &afi) = 0;
  virtual OutputRetCode::CloseRes close() = 0;

  virtual OutputRetCode::UnlockRes unlock() = 0;
  virtual OutputRetCode::LockRes lock() = 0;

  virtual OutputRetCode::WriteRes write(const char *buf, int count) = 0;

  virtual OutputRetCode::StopRes stop() = 0;
  virtual OutputRetCode::PauseRes pause() = 0;
  virtual OutputRetCode::UnpauseRes unpause() = 0;

  virtual void change_device(const char *device) = 0;

  virtual Enum::OutputType get_output_type() const = 0;
  virtual Enum::OutputDeviceType get_output_device_type() const = 0;
};

class AlsaOutput : public Output {
public:
  OutputRetCode::InitRes init(const char *device) override;
  OutputRetCode::ExitRes exit() override;

  OutputRetCode::OpenRes open(const Audio::FormatInfo &afi) override;
  OutputRetCode::CloseRes close() override;

  OutputRetCode::UnlockRes unlock() override;
  OutputRetCode::LockRes lock() override;

  OutputRetCode::WriteRes write(const char *buf, int count) override;

  OutputRetCode::StopRes stop() override;
  OutputRetCode::PauseRes pause() override;
  OutputRetCode::UnpauseRes unpause() override;

  void change_device(const char *device) override;

  Enum::OutputType get_output_type() const override;
  Enum::OutputDeviceType get_output_device_type() const override;

private:
  const char *dev = nullptr;

  int fsize = 0;
  bool can_pause = false;
  snd_pcm_t *handle = nullptr;
  snd_pcm_hw_params_t *params = nullptr;
  snd_pcm_status_t *status = nullptr;
  snd_pcm_format_t fmt = SND_PCM_FORMAT_UNKNOWN;

  int set_hw_params(const Audio::FormatInfo &afi);
};

class OutputFactory {
public:
  static std::unique_ptr<Output> create(const Enum::OutputType type) {
    if (type == Enum::OutputType::ALSA) {
      return std::make_unique<AlsaOutput>();
    } else {
      return nullptr;
    }
  }
};
