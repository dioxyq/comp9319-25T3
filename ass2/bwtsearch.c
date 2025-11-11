#include "bwt-util.h"
#include <string.h>

size_t lf_i(RLFM *rlfm, size_t i, unsigned char code, int is_lst) {
    size_t cs_c = code == 0 ? 1 : rlfm->Cs[code - 1];
    if (code != code_from_l_pos(rlfm, i)) {
        return select_b_indexed(
                   rlfm->Bp,
                   cs_c + 1 +
                       rank_s(rlfm->S, rank_b_indexed(rlfm->B, i), code)) -
               1;
    } else {
        return select_b_indexed(
                   rlfm->Bp,
                   cs_c + rank_s(rlfm->S, rank_b_indexed(rlfm->B, i), code)) +
               i - select_b_indexed(rlfm->B, rank_b_indexed(rlfm->B, i));
    }
}

size_t search(RLFM *rlfm, FILE *file, char *search_term, size_t len) {
    char c = search_term[len - 1];
    unsigned char code =
        (c == ENCODING[1]) + 2 * (c == ENCODING[2]) + 3 * (c == ENCODING[3]);
    size_t fst =
        select_b_indexed(rlfm->Bp, (code == 0 ? 1 : rlfm->Cs[code - 1]) + 1);
    size_t lst = select_b_indexed(rlfm->Bp, rlfm->Cs[code] + 1) - 1;

    for (int i = len - 2; i >= 0; --i) {
        c = search_term[i];
        code = (c == ENCODING[1]) + 2 * (c == ENCODING[2]) +
               3 * (c == ENCODING[3]);
        fst = lf_i(rlfm, fst - 1, code, 0) + 1;
        lst = lf_i(rlfm, lst, code, 1);

        if (lst < fst) {
            /* printf("%c fst: %zu, lst: %zu\n", c, fst, lst); */
            return 0;
        }
    }
    /* printf("%c fst: %zu, lst: %zu\n", c, fst, lst); */
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

    RLFM *rlfm = read_rlfm(file);

    // min 1 char, max 100 chars (from spec)
    char *line = NULL;
    size_t len = 128;
    for (size_t n = getline(&line, &len, stdin); n != -1;
         n = getline(&line, &len, stdin)) {
        size_t count = search(rlfm, file, line, n - 1);
        printf("%zu\n", count);
    }

    print_rlfm(rlfm);
    print_rlfm_s(rlfm->S);
    print_rlfm_b(rlfm->B);
    print_rlfm_b(rlfm->Bp);

    free(line);
    free_rlfm(rlfm);
}
