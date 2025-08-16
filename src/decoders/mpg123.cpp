#include "../decoder.hpp"
#include <iostream>
#include <mpg123.h>

MPG123Decoder::MPG123Decoder() {
  int init_rc = mpg123_init();
  if (init_rc != MPG123_OK) {
    std::cerr << "MPG init error: " << init_rc << '\n';
    handle = nullptr;
  }

  int err_rc = 0;
  mpg123_handle *h = mpg123_new(NULL, &err_rc);

  if (err_rc != 0) {
    std::cerr << "Cannot open new handle: " << err_rc << '\n';
    handle = nullptr;
  }

  handle = h;
}

MPG123Decoder::~MPG123Decoder() {
  if (handle) {
    mpg123_delete(handle);
    handle = nullptr;
    mpg123_exit();
  }
}

bool MPG123Decoder::is_initialized() { return handle != nullptr; }

DecoderRetCode::OpenRes
MPG123Decoder::open(const std::filesystem::path filepath) {
  if (handle == nullptr) {
    return DecoderRetCode::OpenRes::EmptyHandle;
  }

  int rc = mpg123_open(handle, filepath.c_str());
  if (rc != MPG123_OK) {
    return DecoderRetCode::OpenRes::Error;
  }

  return DecoderRetCode::OpenRes::Success;
}

DecoderRetCode::ReadRes MPG123Decoder::read(char *buf, int bufsize,
                                            size_t &done) {
  if (handle == nullptr) {
    return DecoderRetCode::ReadRes::EmptyHandle;
  }

  int rc = mpg123_read(handle, buf, bufsize, &done);
  if (rc != MPG123_OK) {
    return DecoderRetCode::ReadRes::Error;
  }

  return DecoderRetCode::ReadRes::Success;
}

DecoderRetCode::CloseRes MPG123Decoder::close() {
  if (handle == nullptr) {
    return DecoderRetCode::CloseRes::EmptyHandle;
  }

  int rc = mpg123_close(handle);
  if (rc != MPG123_OK) {
    return DecoderRetCode::CloseRes::Error;
  }

  return DecoderRetCode::CloseRes::Success;
}

DecoderRetCode::GetFmtRes MPG123Decoder::get_format(Audio::FormatInfo &afi) {
  if (handle == nullptr) {
    return DecoderRetCode::GetFmtRes::EmptyHandle;
  }

  long rate;
  int channels, encoding;

  int rc = mpg123_getformat(handle, &rate, &channels, &encoding);
  if (rc != MPG123_OK) {
    return DecoderRetCode::GetFmtRes::Error;
  }

  afi.rate = rate;
  afi.channels = channels;
  afi.encoding = encoding;

  afi.bits = mpg123_encsize(encoding) * 8;
  afi.is_signed = (encoding & MPG123_ENC_SIGNED) ? 1 : 0;
  afi.is_bigendian = (encoding & MPG123_BIG_ENDIAN) ? 1 : 0;
  afi.frame_size = channels * (afi.bits / 8);

  return DecoderRetCode::GetFmtRes::Success;
}

DecoderRetCode::SetFmtRes
MPG123Decoder::set_format(const Audio::FormatInfo &afi) {
  if (handle == nullptr) {
    return DecoderRetCode::SetFmtRes::EmptyHandle;
  }

  int rc;

  rc = mpg123_format_none(handle);
  if (rc != MPG123_OK) {
    return DecoderRetCode::SetFmtRes::SetNoneFmtError;
  }

  rc = mpg123_format(handle, afi.rate, afi.channels, afi.encoding);
  if (rc != MPG123_OK) {
    return DecoderRetCode::SetFmtRes::SetFmtError;
  }

  return DecoderRetCode::SetFmtRes::Success;
}
