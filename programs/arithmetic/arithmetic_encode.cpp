#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>

using namespace std;

int main(int argc, char **argv) {
    cout << setprecision(10);

    auto filename = argv[1];
    ifstream file(filename);
    string s;
    getline(file, s);
    cout << s << endl;

    string s2 = s;
    sort(s2.begin(), s2.end());
    map<char, pair<double, double>> ranges{};
    double prob = 1.0 / s2.length();
    for (int i = 0; i < s2.length(); ++i) {
        if (ranges.contains(s2[i])) {
            ranges[s2[i]].second += prob;
        } else {
            ranges[s2[i]] = pair(prob * i, prob * (i + 1));
        }
    }

    double low = 0.0;
    double high = 1.0;
    for (const auto c : s) {
        double code_range = high - low;
        const double idx = c;
        high = low + code_range * ranges[c].second;
        low = low + code_range * ranges[c].first;
        cout << c << ' ' << low << ' ' << high << endl;
    }
    cout << low << ' ' << high << endl;
}
