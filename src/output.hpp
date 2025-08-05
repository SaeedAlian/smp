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

enum class PauseRes {
  Success = 0,
};

enum class UnpauseRes {
  Success = 0,
};

} // namespace OutputRetCode

class AlsaOutput {
public:
  AlsaOutput();
  ~AlsaOutput();

  OutputRetCode::InitRes init();
  OutputRetCode::ExitRes exit();

  OutputRetCode::OpenRes open(const Audio::FormatInfo &afi);
  OutputRetCode::CloseRes close();

  OutputRetCode::UnlockRes unlock();
  OutputRetCode::LockRes lock();

  OutputRetCode::WriteRes write(const char *buf, int count);

  OutputRetCode::PauseRes pause();
  OutputRetCode::UnpauseRes unpause();

private:
  int current_channels;
  int fsize;
  bool can_pause;
  snd_pcm_t *handle = nullptr;
  snd_pcm_hw_params_t *params = nullptr;
  snd_pcm_status_t *status = nullptr;
  snd_pcm_format_t fmt;

  int set_hw_params(const Audio::FormatInfo &afi);
};
