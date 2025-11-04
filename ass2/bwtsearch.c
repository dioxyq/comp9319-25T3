#include "bwt-util.h"
#include <string.h>

size_t search(RLFM *rlfm, FILE *file, char *search_term) { return 0; }

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

    RLFM *rlfm = read_rlfm(file);

    // min 1 char, max 100 chars (from spec)
    char *line = NULL;
    size_t len = 128;
    while (getline(&line, &len, stdin) != -1) {
        size_t count = search(rlfm, file, line);
        printf("%zu\n", count);
    }

    free_rlfm(rlfm);
}
