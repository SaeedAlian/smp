#include "../output.hpp"

OutputRetCode::InitRes AlsaOutput::init(const char *device) {
  int rc;

  rc = snd_pcm_status_malloc(&status);
  if (rc < 0) {
    return OutputRetCode::InitRes::Error;
  }

  dev = strdup(device);

  return OutputRetCode::InitRes::Success;
}

OutputRetCode::ExitRes AlsaOutput::exit() {
  if (status) {
    snd_pcm_status_free(status);
    status = nullptr;
  }
  return OutputRetCode::ExitRes::Success;
}

OutputRetCode::OpenRes AlsaOutput::open(const Audio::FormatInfo &afi) {
  int rc;
  fsize = afi.frame_size;

  if (dev == nullptr) {
    rc = snd_pcm_open(&handle, "sysdefault", SND_PCM_STREAM_PLAYBACK, 0);
  } else {
    rc = snd_pcm_open(&handle, dev, SND_PCM_STREAM_PLAYBACK, 0);

    if (rc < 0) {
      rc = snd_pcm_open(&handle, "sysdefault", SND_PCM_STREAM_PLAYBACK, 0);
    }
  }

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
  if (!handle)
    return OutputRetCode::CloseRes::Success;

  int rc;

  rc = snd_pcm_drain(handle);
  if (rc < 0) {
    return OutputRetCode::CloseRes::DrainError;
  }

  rc = snd_pcm_close(handle);
  if (rc < 0) {
    return OutputRetCode::CloseRes::CloseError;
  }

  handle = nullptr;
  return OutputRetCode::CloseRes::Success;
}

OutputRetCode::WriteRes AlsaOutput::write(const char *buf, int count) {
  if (!handle)
    return OutputRetCode::WriteRes::Error;

  int rc, frames;
  int recovered = 0;

  frames = count / fsize;

  rc = snd_pcm_writei(handle, buf, frames);
  if (rc < 0) {
    rc = snd_pcm_recover(handle, rc, 1);
    if (rc < 0 && ((rc != -EINTR && rc != -EPIPE && rc != -ESTRPIPE)))
      return OutputRetCode::WriteRes::Error;

    rc = snd_pcm_writei(handle, buf, frames);
    if (rc < 0)
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

OutputRetCode::StopRes AlsaOutput::stop() {
  if (!handle)
    return OutputRetCode::StopRes::Error;

  int rc;
  rc = snd_pcm_drop(handle);
  if (rc < 0) {
    return OutputRetCode::StopRes::Error;
  }

  rc = snd_pcm_prepare(handle);
  if (rc < 0) {
    return OutputRetCode::StopRes::Error;
  }

  return OutputRetCode::StopRes::Success;
}

OutputRetCode::PauseRes AlsaOutput::pause() {
  int rc;

  if (can_pause) {
    snd_pcm_state_t state = snd_pcm_state(handle);
    switch (state) {
    case SND_PCM_STATE_PREPARED: {
      return OutputRetCode::PauseRes::Success;
    }

    case SND_PCM_STATE_RUNNING: {
      rc = snd_pcm_wait(handle, -1);
      if (rc < 0) {
        return OutputRetCode::PauseRes::Error;
      }

      rc = snd_pcm_pause(handle, 1);
      if (rc < 0) {
        return OutputRetCode::PauseRes::Error;
      }
    }

    default: {
      return OutputRetCode::PauseRes::InvalidState;
    }
    }
  } else {
    rc = snd_pcm_drop(handle);
    if (rc < 0) {
      return OutputRetCode::PauseRes::Error;
    }
    return OutputRetCode::PauseRes::Dropped;
  }

  return OutputRetCode::PauseRes::Success;
}

OutputRetCode::UnpauseRes AlsaOutput::unpause() {
  int rc;

  if (can_pause) {
    snd_pcm_state_t state = snd_pcm_state(handle);
    switch (state) {
    case SND_PCM_STATE_PREPARED: {
      return OutputRetCode::UnpauseRes::Success;
    }

    case SND_PCM_STATE_PAUSED: {
      rc = snd_pcm_wait(handle, -1);
      if (rc < 0) {
        return OutputRetCode::UnpauseRes::Error;
      }

      rc = snd_pcm_pause(handle, 0);
      if (rc < 0) {
        return OutputRetCode::UnpauseRes::Error;
      }
    }

    default: {
      return OutputRetCode::UnpauseRes::InvalidState;
    }
    }
  } else {
    rc = snd_pcm_prepare(handle);
    if (rc < 0) {
      return OutputRetCode::UnpauseRes::Error;
    }
    return OutputRetCode::UnpauseRes::Prepared;
  }

  return OutputRetCode::UnpauseRes::Success;
}

void AlsaOutput::change_device(const char *device) { dev = strdup(device); }

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

Enum::OutputType AlsaOutput::get_output_type() const {
  return Enum::OutputType::ALSA;
}

Enum::OutputDeviceType AlsaOutput::get_output_device_type() const {
  return output_device_enum(dev);
}
