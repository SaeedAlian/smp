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
  Error,
};

}; // namespace DecoderRetCode

class Decoder {
public:
  virtual ~Decoder() = default;

  virtual bool is_initialized() const = 0;

  virtual DecoderRetCode::OpenRes
  open(const std::filesystem::path &filepath) = 0;
  virtual DecoderRetCode::CloseRes close() = 0;

  virtual DecoderRetCode::ReadRes read(char *buf, int bufsize,
                                       size_t &done) = 0;

  virtual DecoderRetCode::SeekRes seek_set(double offset) = 0;
  virtual DecoderRetCode::SeekRes seek_cur(double offset) = 0;
  virtual DecoderRetCode::SeekRes seek_end(double offset) = 0;

  virtual int64_t tell() = 0;

  virtual DecoderRetCode::GetFmtRes get_format(Audio::FormatInfo &afi) = 0;
  virtual DecoderRetCode::SetFmtRes
  set_format(const Audio::FormatInfo &afi) = 0;

  virtual Enum::DecoderType get_decoder_type() const = 0;
};

class MPG123Decoder : public Decoder {
public:
  MPG123Decoder();
  ~MPG123Decoder() override;

  bool is_initialized() const override;

  DecoderRetCode::OpenRes open(const std::filesystem::path &filepath) override;
  DecoderRetCode::CloseRes close() override;

  DecoderRetCode::ReadRes read(char *buf, int bufsize, size_t &done) override;

  DecoderRetCode::SeekRes seek_set(double offset) override;
  DecoderRetCode::SeekRes seek_cur(double offset) override;
  DecoderRetCode::SeekRes seek_end(double offset) override;

  int64_t tell() override;

  DecoderRetCode::GetFmtRes get_format(Audio::FormatInfo &afi) override;
  DecoderRetCode::SetFmtRes set_format(const Audio::FormatInfo &afi) override;

  Enum::DecoderType get_decoder_type() const override;

private:
  mpg123_handle *handle = nullptr;

  DecoderRetCode::SeekRes seek(double offset, int whence);
};

class DecoderFactory {
public:
  static std::unique_ptr<Decoder> create(const std::string ext) {
    if (ext == ".mp3") {
      return std::make_unique<MPG123Decoder>();
    } else {
      return nullptr;
    }
  }

  static std::unique_ptr<Decoder> create(const Enum::FileType filetype) {
    if (filetype == Enum::FileType::MP3) {
      return std::make_unique<MPG123Decoder>();
    } else {
      return nullptr;
    }
  }
};
