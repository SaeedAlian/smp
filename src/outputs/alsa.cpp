#include "../output.hpp"

AlsaOutput::AlsaOutput() {}

AlsaOutput::~AlsaOutput() {}

OutputRetCode::InitRes AlsaOutput::init() {
  int rc;

  rc = snd_pcm_status_malloc(&status);
  if (rc < 0) {
    return OutputRetCode::InitRes::Error;
  }

  return OutputRetCode::InitRes::Success;
}

OutputRetCode::ExitRes AlsaOutput::exit() {
  snd_pcm_status_free(status);
  return OutputRetCode::ExitRes::Success;
}

OutputRetCode::OpenRes AlsaOutput::open(const Audio::FormatInfo &afi) {
  int rc;
  fsize = afi.frame_size;

  rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
  if (rc < 0) {
    return OutputRetCode::OpenRes::OpenError;
  }

  rc = set_hw_params(afi);
  if (rc < 0) {
    snd_pcm_close(handle);
    return OutputRetCode::OpenRes::SetParamsError;
  }

  rc = snd_pcm_prepare(handle);
  if (rc < 0) {
    snd_pcm_close(handle);
    return OutputRetCode::OpenRes::PrepareError;
  }

  return OutputRetCode::OpenRes::Success;
}

OutputRetCode::CloseRes AlsaOutput::close() {
  int rc;

  rc = snd_pcm_drain(handle);
  if (rc < 0) {
    return OutputRetCode::CloseRes::DrainError;
  }

  rc = snd_pcm_close(handle);
  if (rc < 0) {
    return OutputRetCode::CloseRes::CloseError;
  }

  return OutputRetCode::CloseRes::Success;
}

OutputRetCode::WriteRes AlsaOutput::write(const char *buf, int count) {
  int rc, len;
  int recovered = 0;

  len = count / fsize;

  rc = snd_pcm_writei(handle, buf, len);

  if (rc < 0) {
    return OutputRetCode::WriteRes::Error;
  }

  return OutputRetCode::WriteRes::Success;
}

OutputRetCode::LockRes AlsaOutput::lock() {

  return OutputRetCode::LockRes::Success;
}

OutputRetCode::UnlockRes AlsaOutput::unlock() {

  return OutputRetCode::UnlockRes::Success;
}

OutputRetCode::PauseRes AlsaOutput::pause() {
  return OutputRetCode::PauseRes::Success;
}

OutputRetCode::UnpauseRes AlsaOutput::unpause() {
  return OutputRetCode::UnpauseRes::Success;
}

int AlsaOutput::set_hw_params(const Audio::FormatInfo &afi) {
  unsigned int max_buf_time = 300 * 1000; // 300ms
  int rc, direction;

  unsigned int rate = afi.rate;

  snd_pcm_hw_params_malloc(&params);
  rc = snd_pcm_hw_params_any(handle, params);
  if (rc < 0)
    goto error;

  rc = snd_pcm_hw_params_set_buffer_time_max(handle, params, &max_buf_time,
                                             &direction);
  if (rc < 0)
    goto error;

  can_pause = (bool)snd_pcm_hw_params_can_pause(params);

  rc = snd_pcm_hw_params_set_access(handle, params,
                                    SND_PCM_ACCESS_RW_INTERLEAVED);
  if (rc < 0)
    goto error;

  fmt = snd_pcm_build_linear_format(afi.bits, afi.bits, afi.is_signed ? 0 : 1,
                                    afi.is_bigendian);

  rc = snd_pcm_hw_params_set_format(handle, params, fmt);
  if (rc < 0)
    goto error;

  rc = snd_pcm_hw_params_set_channels(handle, params, afi.channels);
  if (rc < 0)
    goto error;

  rc = snd_pcm_hw_params_set_rate_near(handle, params, &rate, &direction);
  if (rc < 0)
    goto error;

  rc = snd_pcm_hw_params(handle, params);
  if (rc < 0)
    goto error;

error:
  snd_pcm_hw_params_free(params);
  return rc;
}
