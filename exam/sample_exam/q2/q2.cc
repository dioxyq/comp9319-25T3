#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

auto rank(std::string &s, int pos) {
    char c = s[pos];
    int count = 0;
    for (int i = 0; i <= pos; ++i) {
        if (s[i] == c) {
            ++count;
        }
    }
    return count;
}

auto select(std::string &s, char c, int count) {
    int sum = 0;
    int i = 0;
    while (sum < count) {
        if (s[i] == c) {
            ++sum;
        }
        ++i;
    }
    return i - 1;
}

auto main(int argc, char **argv) -> int {
    std::string filename = argv[1];
    std::ifstream file(filename);

    std::ostringstream stream;
    stream << file.rdbuf();
    std::string l = stream.str();

    std::string f = l;
    std::sort(f.begin(), f.end());

    std::string out(l.length(), ' ');

    int fi = 0;
    int li = l.find('\n');
    int i = l.length() - 1;
    do {
        out[i] = l[li];
        li = fi;
        fi = select(f, l[li], rank(l, li));
        --i;
    } while (fi != 0);
    std::cout << out;
}
