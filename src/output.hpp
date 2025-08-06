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

class AlsaOutput {
public:
  OutputRetCode::InitRes init(const char *device);
  OutputRetCode::ExitRes exit();

  OutputRetCode::OpenRes open(const Audio::FormatInfo &afi);
  OutputRetCode::CloseRes close();

  OutputRetCode::UnlockRes unlock();
  OutputRetCode::LockRes lock();

  OutputRetCode::WriteRes write(const char *buf, int count);

  OutputRetCode::StopRes stop();
  OutputRetCode::PauseRes pause();
  OutputRetCode::UnpauseRes unpause();

  void change_device(const char *device);

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
