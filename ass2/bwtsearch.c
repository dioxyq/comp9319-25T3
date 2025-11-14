#include "bwt-util.h"
#include <string.h>

typedef struct {
    uint32_t *Occ;
    size_t gap_size;
    uint32_t C[4];
} FM;

FM *read_fm(FILE *file) {
    // TODO:
    return NULL;
}

size_t lf_i(FM *fm, size_t i, unsigned char code) {
    // TODO: implement
    return 0;
}

size_t search(FM *fm, FILE *file, char *search_term, size_t len) {
    char c = search_term[len - 1];
    unsigned char code =
        (c == ENCODING[1]) + 2 * (c == ENCODING[2]) + 3 * (c == ENCODING[3]);
    size_t fst = 0;
    size_t lst = 0;

    for (int i = len - 2; i >= 0; --i) {
        c = search_term[i];
        code = (c == ENCODING[1]) + 2 * (c == ENCODING[2]) +
               3 * (c == ENCODING[3]);
        fst = lf_i(fm, fst - 1, code) + 1;
        lst = lf_i(fm, lst, code);

        if (lst < fst) {
            return 0;
        }
    }
    return lst - fst + 1;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("usage: bwtdecode FILE\n");
        return 1;
    }

    FILE *file = fopen(argv[1], "rb");
    if (file == NULL) {
        printf("input file not found\n");
        return 1;
    }

    FM *fm = read_fm(file);

    // min 1 char, max 100 chars (from spec)
    char *line = NULL;
    size_t len = 128;
    for (size_t n = getline(&line, &len, stdin); n != -1;
         n = getline(&line, &len, stdin)) {
        size_t count = search(fm, file, line, n - 1);
        printf("%zu\n", count);
    }
    free(line);
}
