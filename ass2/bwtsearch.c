#include "bwt-util.h"
#include <string.h>

static const size_t GAP_SIZE = 128;
static const double OCC_SIZE_RATIO_HEURISTIC = 1.8;

typedef struct {
    unsigned int occ[3];
    unsigned int offset;
} Occ;

typedef struct {
    Occ *Occ;
    size_t gap_size;
    size_t len;
    size_t end;
    uint32_t C[4];
} FM;

void print_fm(FM *fm) {
    printf("gap_size: %zu, len: %zu, ", fm->gap_size, fm->len);
    printf("C: [A: %d, C: %d, G: %d, T: %d]\n", fm->C[0], fm->C[1], fm->C[2],
           fm->C[3]);
    for (size_t i = 0; i < fm->len; ++i) {
        Occ occ = fm->Occ[i];
        printf("A: %d, C: %d, G: %d, T: %zu, offset: %d\n", occ.occ[0],
               occ.occ[1], occ.occ[2],
               i + (i < fm->end) -
                   (size_t)(occ.occ[0] + occ.occ[1] + occ.occ[2]),
               occ.offset);
    }
}

void free_fm(FM *fm) {
    if (fm == NULL) {
        return;
    }
    if (fm->Occ != NULL) {
        free(fm->Occ);
    }
    free(fm);
}

FM *init_fm(size_t file_size) {
    size_t occ_size = max(MIN_ALLOC, file_size * OCC_SIZE_RATIO_HEURISTIC);

    FM *fm = malloc(sizeof(FM));

    fm->Occ = calloc(occ_size, sizeof(Occ));
    fm->gap_size = GAP_SIZE;
    fm->len = 0;
    fm->end = 0;

    fm->C[0] = 0;
    fm->C[1] = 0;
    fm->C[2] = 0;
    fm->C[3] = 0;

    return fm;
}

FM *read_fm(FILE *file) {
    struct stat stat_buf;
    fstat(fileno(file), &stat_buf);
    size_t file_size = stat_buf.st_size;

    FM *fm = init_fm(file_size);

    char *buf = malloc(READ_BUF_SIZE);
    size_t bytes_to_read = file_size;
    size_t bytes_left = min(READ_BUF_SIZE, bytes_to_read);

    size_t occ_offset = 0;

    Occ prev_occ = {.occ = {0}, .offset = 0};

    while (fread(buf, bytes_left, 1, file)) {
        for (size_t i = 0; i < bytes_left; ++i) {
            unsigned char code = (buf[i] & 0b11100000) >> 5;
            unsigned char run_length = (buf[i] & 0b11111) + 1;

            if (code < 3) {
                fm->C[code + 1] += run_length;
            } else if (code == 4) {
                fm->end = occ_offset;
            }

            for (size_t j = occ_offset; occ_offset < j + run_length;
                 ++occ_offset) {
                if (code < 3) {
                    ++prev_occ.occ[code];
                }
                fm->Occ[occ_offset] = prev_occ;
            }

            ++prev_occ.offset;
        }

        bytes_to_read -= READ_BUF_SIZE;
        bytes_left = min(READ_BUF_SIZE, bytes_to_read);
    }

    free(buf);

    fm->C[0] = 1;
    fm->C[1] += fm->C[0];
    fm->C[2] += fm->C[1];
    fm->C[3] += fm->C[2];

    fm->len = occ_offset;

    fm->Occ = realloc(fm->Occ, fm->len * sizeof(Occ));

    return fm;
}

size_t get_occ(FM *fm, FILE *file, size_t i, unsigned char code) {
    Occ occ = fm->Occ[i];
    if (code < 3) {
        return occ.occ[code];
    }
    return i + (i < fm->end) - (size_t)(occ.occ[0] + occ.occ[1] + occ.occ[2]);
}

size_t lf_i(FM *fm, FILE *file, size_t i, unsigned char code) {
    return fm->C[code] + get_occ(fm, file, i, code);
}

size_t search(FM *fm, FILE *file, char *search_term, size_t len) {
    char c = search_term[len - 1];
    unsigned char code =
        (c == ENCODING[1]) + 2 * (c == ENCODING[2]) + 3 * (c == ENCODING[3]);

    size_t fst = fm->C[code];
    size_t lst = code < 3 ? fm->C[code + 1] - 1 : fm->len - 1;

    for (int i = len - 2; i >= 0; --i) {
        c = search_term[i];
        code = (c == ENCODING[1]) + 2 * (c == ENCODING[2]) +
               3 * (c == ENCODING[3]);
        fst = lf_i(fm, file, fst - 1, code);
        lst = lf_i(fm, file, lst, code) - 1;

        if (lst < fst) {
            return 0;
        }
    }
    return lst - fst + 1;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("usage: bwtsearch FILE\n");
        return 1;
    }

    FILE *file = fopen(argv[1], "rb");
    if (file == NULL) {
        printf("input file not found\n");
        return 1;
    }

    FM *fm = read_fm(file);
    /* print_fm(fm); */

    // min 1 char, max 100 chars (from spec)
    char *line = NULL;
    size_t len = 128;
    for (size_t n = getline(&line, &len, stdin); n != -1;
         n = getline(&line, &len, stdin)) {
        size_t count = search(fm, file, line, n - 1);
        printf("%zu\n", count);
    }
    free(line);

    free_fm(fm);
}
