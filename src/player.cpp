#include "player.hpp"
#include "common/types.hpp"
#include "decoder.hpp"
#include "output.hpp"
#include <filesystem>
#include <fmt/format.h>

Player::Player() {
  AlsaOutput *o = new AlsaOutput;
  MPG123Decoder *d = new MPG123Decoder;

  if (!d->is_initialized()) {
    output = nullptr;
    decoder = nullptr;
  }

  output = o;
  decoder = d;

  o->init("pulse");
}

Player::~Player() {
  if (decoder) {
    decoder->close();
    delete decoder;
  }
  if (output) {
    output->exit();
    delete output;
  }
}

int Player::load(const Entity::File &file) {
  const std::filesystem::path fullpath =
      fmt::format("{}/{}", file.fulldir_path.c_str(), file.filename.c_str());
  decoder->open(fullpath);
  Audio::FormatInfo afi;
  decoder->get_format(afi);
  decoder->set_format(afi);

  output->open(afi);

  return 0;
}

int Player::play() {

  size_t done;
  int bufsize = 8092;
  char *buf = new char[bufsize];
  while (decoder->read(buf, bufsize, done) ==
         DecoderRetCode::ReadRes::Success) {
    output->write(buf, done);
  }

  delete[] buf;

  return 0;
}
