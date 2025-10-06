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
  std::string p;
  char c;
  uint32_t index = 256;
  uint32_t reset_index = 1;
  std::map<uint32_t, std::string> dict;

  (void)reset_frequency;
  (void)reset_index;

  input_file.get(c); // skip first symbol
  output_file << c;
  p = c;

  while (input_file.get(c)) {
    unsigned char uc = static_cast<unsigned char>(c);
    std::string out;

    if (uc < 0x80) { // regular char
      out.push_back(c);
    } else { // 2-byte code
      uint32_t code = 0x3F & uc;
      input_file.get(c);
      reset_index++;
      code <<= 8;
      code |= static_cast<unsigned char>(c);

      if (uc > 0xC0) { // 3-byte code
        input_file.get(c);
        reset_index++;
        code <<= 8;
        code |= static_cast<unsigned char>(c);
      }

      // output_file << '<' << code << '>';

      // lookup code, handle edge case
      out.append(code > index ? p + p[0] : dict[code]);
    }

    output_file << out;
    dict.emplace(index++, p + out[0]);
    p = out;
  }
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
