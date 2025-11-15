#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

static const char ENCODING[5] = {'A', 'C', 'G', 'T', '\n'};
static const size_t KIBIBYTE = 1024;
static const size_t MEBIBYTE = 1024 * 1024;
static const size_t MIN_ALLOC = KIBIBYTE;
static const size_t READ_BUF_SIZE = 64 * KIBIBYTE;
static const size_t GAP_SIZE = 128;
static const double OCC_SIZE_RATIO_HEURISTIC = 1.8;

typedef struct {
    unsigned int occ[3];
    unsigned int offset : 27;
    unsigned int rl_offset : 5;
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
    for (size_t i = 0; i < (fm->len + fm->gap_size - 1) / fm->gap_size; ++i) {
        Occ occ = fm->Occ[i];
        printf("A: %d, C: %d, G: %d, T: %zu, offset: %d, rl_offset: %d\n",
               occ.occ[0], occ.occ[1], occ.occ[2],
               i * fm->gap_size + (i * fm->gap_size < fm->end) -
                   (size_t)(occ.occ[0] + occ.occ[1] + occ.occ[2]),
               occ.offset, occ.rl_offset);
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
    size_t occ_size =
        max(MIN_ALLOC, file_size * OCC_SIZE_RATIO_HEURISTIC / GAP_SIZE);

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
                if (occ_offset % fm->gap_size == 0) {
                    size_t occ_index = occ_offset / fm->gap_size;
                    prev_occ.rl_offset = occ_offset - j;
                    fm->Occ[occ_index] = prev_occ;
                }
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

    fm->Occ = realloc(fm->Occ, ((fm->len + fm->gap_size - 1) / fm->gap_size) *
                                   sizeof(Occ));

    return fm;
}

size_t read_occ_gap(FM *fm, FILE *file, size_t pos, unsigned char code) {
    struct stat stat_buf;
    fstat(fileno(file), &stat_buf);
    size_t file_size = stat_buf.st_size;

    size_t occ_index = pos / fm->gap_size;
    Occ occ = fm->Occ[occ_index];

    size_t occ_offset = occ_index * fm->gap_size;
    size_t chars_to_read = min(pos - occ_offset, file_size - occ.offset) + 1;

    size_t c_count = (code < 3)
                         ? occ.occ[code]
                         : occ_offset + (occ_offset < fm->end) -
                               (size_t)(occ.occ[0] + occ.occ[1] + occ.occ[2]);

    char *buf = malloc(chars_to_read);
    fseek(file, occ.offset, SEEK_SET);
    fread(buf, chars_to_read, 1, file);

    ++occ_offset;

    for (size_t i = 0; i < chars_to_read && occ_offset <= pos; ++i) {
        unsigned char code_i = (buf[i] & 0b11100000) >> 5;
        unsigned char run_length = (buf[i] & 0b11111) + 1;

        if (i == 0) {
            run_length -= occ.rl_offset + 1;
        }

        if (code_i == code) {
            if (occ_offset + run_length > pos) {
                c_count += pos - occ_offset + 1;
                break;
            }
            c_count += run_length;
        }
        occ_offset += run_length;
    }

    free(buf);

    return c_count;
}

size_t lf_i(FM *fm, FILE *file, size_t i, unsigned char code) {
    return fm->C[code] + read_occ_gap(fm, file, i, code);
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

    // printf("i A C G T\n");
    // for (int i = 0; i < 247910; ++i) {
    //     printf("%d ", i);
    //     for (unsigned char code = 0; code < 4; ++code) {
    //         size_t count = read_occ_gap(fm, file, i, code);
    //         printf("%zu ", count);
    //     }
    //     printf("\n");
    // }

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
