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

inline auto out_code(uint32_t code, std::ofstream &output_file) {
  const auto num_bytes = 1 + (code >= 1U << 8) + (code >= 1U << 14);
  if (num_bytes == 2) {
    code |= 0x80U << 8; // begin 1
  } else if (num_bytes == 3) {
    code |= 0xC0U << 16; // begin 11
  }
  const auto code_big_endian = swap_endian(code);
  const auto ptr_offset =
      sizeof(uint32_t) - static_cast<unsigned long>(num_bytes);
  output_file.write(
      reinterpret_cast<const char *>(&code_big_endian) + ptr_offset, num_bytes);
}

// in submission, dict was unintentionally passed by value instead of by
// reference, leading to massive slowdown
inline auto out(std::string p, std::map<std::string, uint32_t> &dict,
                std::ofstream &output_file) {
  if (auto sym = dict.find(p); sym != dict.end()) {
    out_code(sym->second, output_file);
  } else {
    output_file << p;
  } // output p code if in dict, otherwise output p
}

auto lzw_encode(std::ifstream &input_file, std::ofstream &output_file,
                const uint32_t reset_frequency) {
  // output reset frequency
  auto rf_big_endian = swap_endian(reset_frequency);
  output_file.write(reinterpret_cast<const char *>(&rf_big_endian),
                    sizeof rf_big_endian);

  std::string p;
  char c;
  uint32_t index = 256;
  uint32_t reset_index = 1;
  std::map<std::string, uint32_t> dict;

  input_file.get(c); // skip first symbol
  p = c;

  while (input_file.get(c)) {
    // handle dictionary reset
    if (reset_index++ == reset_frequency) {
      reset_index = 1;
      index = 256;
      out(p, dict, output_file);
      dict.clear();
      p = c;
      continue;
    }

    // p in dict
    p.push_back(c); // p becomes pc
    if (dict.find(p) != dict.end()) {
      continue;
    }

    // p not in dict
    if (index < 1UL << 22) {
      dict.insert(std::make_pair(p, index++));
    } // insert into dictionary if not full
    p.pop_back(); // restore old p
    out(p, dict, output_file);
    p = c;
  }

  out(p, dict, output_file);
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

  lzw_encode(input_file, output_file, reset_frequency);
}
