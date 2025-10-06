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

auto lzw_decode(std::ifstream &input_file, std::ofstream &output_file) {
  uint32_t reset_frequency;

  input_file.read(reinterpret_cast<char *>(&reset_frequency),
                  sizeof(reset_frequency));
  reset_frequency = swap_endian(reset_frequency);

  std::string p;
  char c;
  uint32_t index = 256;
  uint32_t reset_index = 1;
  std::map<uint32_t, std::string> dict;

  input_file.get(c); // skip first symbol
  output_file << c;
  p = c;

  while (input_file.get(c)) {
    // handle dictionary reset
    if (reset_index >= reset_frequency) {
      reset_index = 1;
      index = 256;
      dict.clear();
      output_file << c;
      p = c;
      continue;
    }

    unsigned char uc = static_cast<unsigned char>(c);
    std::string out;

    if (uc < 0x80U) { // regular char
      out.push_back(c);
    } else { // 2-byte code
      uint32_t code = 0x3FU & uc;
      input_file.get(c);
      code <<= 8;
      code |= static_cast<unsigned char>(c);

      if (uc > 0xC0U) { // 3-byte code
        input_file.get(c);
        code <<= 8;
        code |= static_cast<unsigned char>(c);
      }

      // lookup code, handle edge case
      out.append(code >= index ? p + p[0] : dict[code]);
    }

    output_file << out;
    reset_index += static_cast<uint32_t>(out.length());
    if (index < 1U << 22) {
      dict.emplace(index++, p + out[0]);
    }
    p = out;
  }
}

auto main(int argc, char *argv[]) -> int {
  // parse args
  if (argc != 3) {
    std::cout << "usage: ldecode INPUT OUTPUT" << '\n';
    return 0;
  }
  std::ifstream input_file(argv[1]);
  std::ofstream output_file(argv[2]);

  lzw_decode(input_file, output_file);
}
