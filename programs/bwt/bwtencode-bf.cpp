#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

using namespace std;

auto main(int argc, char **argv) -> int {
    string filename = argv[1];
    ifstream file(filename);

    ostringstream stream;
    stream << file.rdbuf();
    string l = stream.str();

    vector<string> rotations(l.length());

    for (int i = 0; i < l.length(); ++i) {
        for (int j = 0; j < l.length(); ++j) {
            rotations[j].insert(0, 1, l[j]);
        }
        sort(rotations.begin(), rotations.end());
    }

    string s = *find_if(rotations.begin(), rotations.end(),
                        [](const string &s) { return s.back() == '\n'; });

    std::cout << s;
}
