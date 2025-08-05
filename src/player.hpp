#pragma once
#include "common/types.hpp"
#include "decoder.hpp"
#include "output.hpp"

class Player {
public:
  Player();
  ~Player();

  int load(const Entity::File &file);
  int play();

private:
  AlsaOutput *output = nullptr;
  MPG123Decoder *decoder = nullptr;
};
