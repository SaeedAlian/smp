#pragma once
#include "common/types.hpp"
#include <filesystem>
#include <mpg123.h>

namespace DecoderRetCode {

enum class OpenRes {
  Success = 0,
  EmptyHandle,
  Error,
};

enum class CloseRes {
  Success = 0,
  EmptyHandle,
  Error,
};

enum class SetFmtRes {
  Success = 0,
  EmptyHandle,
  SetNoneFmtError,
  SetFmtError,
};

enum class GetFmtRes {
  Success = 0,
  EmptyHandle,
  Error,
};

enum class ReadRes {
  Success = 0,
  EmptyHandle,
  Error,
};

enum class SeekRes {
  Success = 0,
  EmptyHandle,
};

}; // namespace DecoderRetCode

class MPG123Decoder {
public:
  MPG123Decoder();
  ~MPG123Decoder();

  bool is_initialized();

  DecoderRetCode::OpenRes open(const std::filesystem::path filepath);
  DecoderRetCode::CloseRes close();

  DecoderRetCode::ReadRes read(char *buf, int bufsize, size_t &done);
  DecoderRetCode::SeekRes seek();

  DecoderRetCode::GetFmtRes get_format(Audio::FormatInfo &afi);
  DecoderRetCode::SetFmtRes set_format(const Audio::FormatInfo &afi);

private:
  mpg123_handle *handle = nullptr;
};
