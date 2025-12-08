#include <fstream>
#include <iomanip>
#include <iostream>

int main(int argc, char **argv) {
    std::cout << std::setprecision(10);
    const double RANGE_SIZE = 1.0 / 128.0;

    auto filename = argv[1];
    std::ifstream file(filename);
    std::string s;
    std::getline(file, s);

    std::cout << s << std::endl;

    double low = 0.0;
    double high = 1.0;
    for (const auto c : s) {
        double code_range = high - low;
        const double idx = c;
        high = low + code_range * RANGE_SIZE * (c + 1);
        low = low + code_range * RANGE_SIZE * c;
        std::cout << c << ' ' << low << ' ' << high << std::endl;
    }

    std::cout << low << ' ' << high << std::endl;
    return 0;
}
