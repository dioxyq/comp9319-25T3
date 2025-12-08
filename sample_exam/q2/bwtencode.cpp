#include <fstream>
#include <iostream>
#include <ranges>
#include <sstream>
#include <vector>

using namespace std;

auto main(int argc, char **argv) -> int {
    string filename = argv[1];
    ifstream file(filename);

    ostringstream stream;
    stream << file.rdbuf();
    string s = stream.str();
    size_t len = s.length();
    s += s;

    vector<string_view> rotations{};

    for (int i = 0; i < len; ++i) {
        rotations.emplace_back(string_view(s.begin() + i, s.begin() + len + i));
        sort(rotations.begin(), rotations.end());
    }

    string out = rotations |
                 views::transform([](const auto &r) { return r.back(); }) |
                 ranges::to<string>();

    cout << out;
}
