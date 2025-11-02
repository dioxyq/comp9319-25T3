#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

const char ENCODING[5] = {'A', 'C', 'G', 'T', '\n'};
const size_t KIBIBYTE = 1024;
const size_t MEBIBYTE = 1024 * 1024;
const size_t MIN_ALLOC = MEBIBYTE;
const float BT_RATIO_HEURISTIC = 0.22;
const float ST_RATIO_HEURISTIC = 0.25;
const size_t READ_BUF_SIZE = 64 * KIBIBYTE;

typedef struct {
    char *S;
    char *B;
    char *Bp;
    unsigned int C[4];
    size_t s_len;
    size_t b_len;
    size_t s_end;
    size_t b_end;
} FM_Index;

void print_fm_s(FM_Index *fm) {
    for (size_t i = 0; i < (fm->s_len + 3) / 4; ++i) {
        char byte = fm->S[i];
        char buf[5] = {'\n'};
        int print_end = 0;
        for (int j = 0; j < min(4, fm->s_len - i * 4); ++j) {
            if (i * 4 + j == fm->s_end) {
                buf[j] = '#';
                print_end = 1;
            }
            char code = (byte & (0b11 << (2 * j))) >> (2 * j);
            buf[j + print_end] = ENCODING[code];
        }
        printf("%s", buf);
    }
    printf("\n");
}

void print_fm_b(FM_Index *fm) {
    for (size_t i = 0; i < (fm->b_len + 7) / 8; ++i) {
        char byte = fm->B[i];
        for (int j = 0; j < min(8, fm->b_len - i * 8); ++j) {
            char bit = (byte & (1 << j)) >> j;
            printf("%d", bit);
        }
    }
    printf("\n");
}

void print_fm(FM_Index *fm) {
    printf("s_len: %zu, b_len: %zu, ", fm->s_len, fm->b_len);
    printf("C: [A: %d, C: %d, G: %d, T: %d]\n", fm->C[0], fm->C[1], fm->C[2],
           fm->C[3]);
}

FM_Index *init_fm(size_t file_size) {
    size_t s_size = max(MIN_ALLOC, file_size * ST_RATIO_HEURISTIC);
    size_t b_size = max(MIN_ALLOC, file_size * BT_RATIO_HEURISTIC);
    size_t bp_size = b_size;

    FM_Index *fm = malloc(sizeof(FM_Index));

    fm->S = calloc(s_size, 1);
    fm->B = calloc(b_size, 1);
    fm->Bp = NULL;
    fm->C[0] = 0;
    fm->C[1] = 0;
    fm->C[2] = 0;
    fm->C[3] = 0;
    fm->s_len = 0;
    fm->b_len = 0;
    fm->s_end = 0;
    fm->b_end = 0;

    return fm;
}

void read_fm(FM_Index *fm, FILE *file, size_t file_size) {
    char *buf = malloc(READ_BUF_SIZE);
    size_t bytes_to_read = file_size;
    size_t bytes_left = min(READ_BUF_SIZE, bytes_to_read);

    size_t s_offset = 0;
    char s_bit_offset = 0;
    size_t b_offset = 0;
    char b_bit_offset = 0;

    while (fread(buf, bytes_left, 1, file)) {
        char last_code = -1;
        // set B and S from file data
        for (size_t i = 0; i < bytes_left; ++i) {
            char code = (buf[i] & 0b11100000) >> 5;
            char run_length = (buf[i] & 0b11111) + 1;

            // branch predictor should effectively ignore this
            if (code == 4) {
                fm->s_end = s_offset * 4 + s_bit_offset / 2;
                fm->b_end = b_offset * 8 + b_bit_offset;
                last_code = code;
                continue;
            }

            if (code != last_code) {
                fm->C[code] += 1;

                fm->S[s_offset] |= code << s_bit_offset;
                // branchlessly update s offsets
                s_offset += (s_bit_offset == 6);
                s_bit_offset = (s_bit_offset + 2) * (s_bit_offset < 6);

                last_code = code;

                fm->B[b_offset] |= 1 << b_bit_offset;
                // branchlessly update b offsets for 1
                b_offset += (b_bit_offset == 7);
                b_bit_offset = (b_bit_offset + 1) * (b_bit_offset < 7);
                --run_length;
            }

            // branchlessly update b offsets for 0s
            b_offset += (run_length + b_bit_offset) / 8;
            b_bit_offset = (run_length + b_bit_offset) % 8;
        }

        bytes_to_read -= READ_BUF_SIZE;
        size_t bytes_left = min(READ_BUF_SIZE, bytes_to_read);
    }

    fm->C[1] += fm->C[0];
    fm->C[2] += fm->C[1];
    fm->C[3] += fm->C[2];

    fm->s_len = s_offset * 4 + s_bit_offset / 2;
    fm->b_len = b_offset * 8 + b_bit_offset;

    fm->S = realloc(fm->S, s_offset + 1);
    fm->B = realloc(fm->B, b_offset + 1);

    /* printf("alloc s:%zu b:%zu\n", s_offset + 1, b_offset + 1); */

    free(buf);
}

void derive_bp(FM_Index *fm) {
    // TODO:
}

void decode_out(FM_Index *fm, FILE *file) {
    // TODO:
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("usage: bwtdecode INPUT_FILE OUTPUT_FILE\n");
        return 1;
    }

    FILE *input_file = fopen(argv[1], "rb");
    if (input_file == NULL) {
        printf("input file not found\n");
        return 1;
    }

    struct stat stat_buf;
    fstat(fileno(input_file), &stat_buf);
    size_t file_size = stat_buf.st_size;

    FM_Index *fm = init_fm(file_size);
    read_fm(fm, input_file, file_size);
    fclose(input_file);

    derive_bp(fm);

    FILE *output_file = fopen(argv[2], "w");
    decode_out(fm, output_file);
    fclose(output_file);

    /* print_fm(fm); */
    /* print_fm_s(fm); */
    /* print_fm_b(fm); */

    free(fm->S);
    free(fm->B);
    free(fm);
}
