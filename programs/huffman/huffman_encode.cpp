#include <deque>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

using namespace std;

struct node {
    size_t value;
    optional<char> c;
    node *left;
    node *right;
};

auto leaf(size_t value, char c) {
    return node{
        .value = value,
        .c = optional(c),
        .left = NULL,
        .right = NULL,
    };
}

auto inner(size_t value, node *left, node *right) {
    return node{
        .value = value,
        .c = nullopt,
        .left = left,
        .right = right,
    };
}

auto print_tree(node *tree, string path = "") -> void {
    if (tree->c.has_value()) {
        cout << '(' << tree->c.value() << ',' << path << ')' << endl;
        return;
    }
    print_tree(tree->left, path + '0');
    print_tree(tree->right, path + '1');
}

auto main(int argc, char **argv) -> int {
    string filename = argv[1];
    ifstream file(filename);

    ostringstream stream;
    stream << file.rdbuf();
    string s = stream.str();

    map<char, int> counter;
    for (const auto c : s) {
        ++counter[c];
    }

    vector<node> nodes{};
    nodes.reserve(counter.size() * 2);
    deque<node *> tree{};
    for (const auto &[c, v] : counter) {
        auto &n = nodes.emplace_back(leaf(v, c));
        tree.push_back(&n);
    }

    while (tree.size() > 1) {
        sort(tree.begin(), tree.end(),
             [](auto a, auto b) { return a->value < b->value; });

        auto fst = tree.front();
        tree.pop_front();
        auto snd = tree.front();
        tree.pop_front();

        auto &n = nodes.emplace_back(inner(fst->value + snd->value, snd, fst));
        tree.push_front(&n);
    }

    print_tree(tree.front());
}
