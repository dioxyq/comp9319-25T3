#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>

inline auto swap_endian(uint32_t x) -> uint32_t {
  uint32_t r = 0;
  for (auto i = 0; i <= 24; i += 8) {
    r |= ((x & (0xFFU << i)) >> i) << (24 - i);
  }
  return r;
}

auto lzw_decode(std::ifstream &input_file, std::ofstream &output_file,
                const uint32_t reset_frequency) {
  (void)input_file;
  (void)output_file;
  (void)reset_frequency;

  std::cout << reset_frequency;
}

auto main(int argc, char *argv[]) -> int {
  // parse args
  if (argc != 3) {
    std::cout << "usage: ldencode INPUT OUTPUT" << '\n';
    return 0;
  }
  std::ifstream input_file(argv[1]);
  std::ofstream output_file(argv[2]);
  uint32_t reset_frequency;

  input_file.read(reinterpret_cast<char *>(&reset_frequency),
                  sizeof(reset_frequency));
  reset_frequency = swap_endian(reset_frequency);

  lzw_decode(input_file, output_file, reset_frequency);
}
