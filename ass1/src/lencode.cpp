#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>

inline auto out_code_debug(uint32_t code, std::ofstream &output_file) {
  output_file << '<' << code << '>';
}

inline auto out_code(uint32_t code, std::ofstream &output_file) {
  auto small = static_cast<char>(0xFFU & code);
  auto mid = static_cast<char>(((0xFFU << 8) & code) >> 8);
  if (code < 1UL << 14) {
    mid |= static_cast<char>(0x80U);
  } else {
    auto big = static_cast<char>(0xC0U | (((0xFFU << 16) & code) >> 16));
    output_file << big;
  }
  output_file << mid << small;
}

auto lzw(std::ifstream &input_file, std::ofstream &output_file,
         const uint32_t reset_frequency) {
  output_file << static_cast<char>((reset_frequency >> 24) & 0xFFU)
              << static_cast<char>((reset_frequency >> 16) & 0xFFU)
              << static_cast<char>((reset_frequency >> 8) & 0xFFU)
              << static_cast<char>(reset_frequency & 0xFFU);

  std::string p;
  char c;
  uint32_t index = 256;
  uint32_t reset_index = 1;
  std::map<std::string, uint32_t> dict;

  input_file.get(c); // skip first symbol
  p = c;

  while (input_file.get(c)) {
    if (reset_index++ == reset_frequency) {
      reset_index = 0;
      index = 256;
      dict.clear();
      output_file << p;
      p = c;
      continue;
    }

    p.push_back(c); // p becomes pc

    if (dict.find(p) != dict.end()) {
      continue;
    }

    if (index < 1UL << 22) {
      dict.insert(std::make_pair(p, index++));
    } // insert into dictionary if not full

    p.pop_back(); // restore old p

    if (auto sym = dict.find(p); sym != dict.end()) {
      out_code(sym->second, output_file);
    } else {
      output_file << p;
    } // print p code if in dict, otherwise print p

    p = c;
  }

  output_file << p;
}

auto main(int argc, char *argv[]) -> int {
  // parse args
  if (argc != 4) {
    std::cout << "usage: lencode INPUT OUTPUT RESET_FREQ" << '\n';
    return 0;
  }
  std::ifstream input_file(argv[1]);
  std::ofstream output_file(argv[2]);
  auto reset_frequency = static_cast<uint32_t>(std::stoul(argv[3]));

  lzw(input_file, output_file, reset_frequency);
}
