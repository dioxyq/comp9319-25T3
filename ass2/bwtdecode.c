#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

static const char ENCODING[5] = {'A', 'C', 'G', 'T', '\n'};
static const size_t KIBIBYTE = 1024;
static const size_t MEBIBYTE = 1024 * 1024;
static const size_t MIN_ALLOC = MEBIBYTE;
static const float BT_RATIO_HEURISTIC = 0.22;
static const float ST_RATIO_HEURISTIC = 0.25;
static const size_t READ_BUF_SIZE = 64 * KIBIBYTE;
static const unsigned char COUNT_ONES[256] = {
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 1, 2, 2, 3, 2, 3, 3, 4,
    2, 3, 3, 4, 3, 4, 4, 5, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 1, 2, 2, 3, 2, 3, 3, 4,
    2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6,
    4, 5, 5, 6, 5, 6, 6, 7, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 2, 3, 3, 4, 3, 4, 4, 5,
    3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6,
    4, 5, 5, 6, 5, 6, 6, 7, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8};

typedef struct {
    unsigned char *data;
    size_t size;
    size_t len;
    size_t end;
} Index;

static const Index NEW_INDEX = {.data = NULL, .size = 0, .len = 0, .end = 0};

typedef struct {
    Index S;
    Index B;
    Index Bp;
    unsigned int Cs[4];
} RLFM;

// pos must be in bounds
size_t rank_b(Index *index, size_t pos) {
    size_t count = 0;
    for (size_t j = 0; j < pos / 8; j++) {
        count += COUNT_ONES[index->data[j]];
    }
    // count remaining bits from i - 7 to i
    count += COUNT_ONES[0xFF >> (7 - (pos % 8)) & index->data[pos / 8]];
    return count;
}

// count must be less than s->len
size_t select_b(Index *index, size_t count) {
    size_t j = 0;
    size_t sum = 0;
    for (; sum < count; j++) {
        sum += COUNT_ONES[index->data[j]];
    }
    size_t pos = (j - 1) * 8;
    if (sum > count) {
        sum -= COUNT_ONES[index->data[j]];
        for (int i = 0; i < 8; i++) {
            // count remaining bits from i - 7 to i
            size_t n = COUNT_ONES[0xFF >> (7 - i) & index->data[j]];
            if (n == count - sum) {
                pos += i;
                break;
            }
        }
    }
    return pos;
}

void print_rlfm_s(RLFM *rlfm) {
    for (size_t i = 0; i < (rlfm->S.len + 3) / 4; ++i) {
        char byte = rlfm->S.data[i];
        char buf[5] = {'\0'};
        for (int j = 0; j < min(4, rlfm->S.len - i * 4); ++j) {
            char code = (byte & (0b11 << (2 * j))) >> (2 * j);
            if (i * 4 + j == rlfm->S.end) {
                code = 4;
            }
            buf[j] = ENCODING[code];
        }
        printf("%s", buf);
    }
    printf("\n");
}

void print_rlfm_b(RLFM *rlfm) {
    for (size_t i = 0; i < (rlfm->B.len + 7) / 8; ++i) {
        char byte = rlfm->B.data[i];
        for (int j = 0; j < min(8, rlfm->B.len - i * 8); ++j) {
            char bit = (byte & (1 << j)) >> j;
            printf("%d", bit);
        }
    }
    printf("\n");
}

void print_rlfm(RLFM *rlfm) {
    printf("s_size: %zu, b_size: %zu, s_len: %zu, b_len: %zu, ", rlfm->S.size,
           rlfm->B.size, rlfm->S.len, rlfm->B.len);
    printf("C: [A: %d, C: %d, G: %d, T: %d]\n", rlfm->Cs[0], rlfm->Cs[1],
           rlfm->Cs[2], rlfm->Cs[3]);
}

RLFM *init_rlfm(size_t file_size) {
    size_t s_size = max(MIN_ALLOC, file_size * ST_RATIO_HEURISTIC);
    size_t b_size = max(MIN_ALLOC, file_size * BT_RATIO_HEURISTIC);
    size_t bp_size = b_size;

    RLFM *rlfm = malloc(sizeof(RLFM));

    rlfm->S = NEW_INDEX;
    rlfm->S.data = calloc(s_size, 1);
    rlfm->B = NEW_INDEX;
    rlfm->B.data = calloc(b_size, 1);
    rlfm->Bp = NEW_INDEX;
    rlfm->Cs[0] = 0;
    rlfm->Cs[1] = 0;
    rlfm->Cs[2] = 0;
    rlfm->Cs[3] = 0;

    return rlfm;
}

void read_rlfm(RLFM *rlfm, FILE *file, size_t file_size) {
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

            if (code != last_code) {
                last_code = code;

                // branch predictor should effectively ignore this
                if (code == 4) {
                    rlfm->S.end = s_offset * 4 + s_bit_offset / 2;
                    rlfm->B.end = b_offset * 8 + b_bit_offset;
                    code = 0;
                }

                rlfm->Cs[code] += 1;

                rlfm->S.data[s_offset] |= code << s_bit_offset;
                // branchlessly update s offsets
                s_offset += (s_bit_offset == 6);
                s_bit_offset = (s_bit_offset + 2) * (s_bit_offset < 6);

                rlfm->B.data[b_offset] |= 1 << b_bit_offset;
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

    rlfm->Cs[1] += rlfm->Cs[0];
    rlfm->Cs[2] += rlfm->Cs[1];
    rlfm->Cs[3] += rlfm->Cs[2];

    rlfm->S.size = s_offset + 1;
    rlfm->B.size = b_offset + 1;
    rlfm->S.len = s_offset * 4 + s_bit_offset / 2;
    rlfm->B.len = b_offset * 8 + b_bit_offset;

    rlfm->S.data = realloc(rlfm->S.data, rlfm->S.size);
    rlfm->B.data = realloc(rlfm->B.data, rlfm->B.size);

    free(buf);
}

void derive_bp(RLFM *fm) {
    // TODO:
}

void decode_out(RLFM *fm, FILE *file) {
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

    RLFM *rlfm = init_rlfm(file_size);
    read_rlfm(rlfm, input_file, file_size);
    fclose(input_file);

    derive_bp(rlfm);

    FILE *output_file = fopen(argv[2], "w");
    decode_out(rlfm, output_file);
    fclose(output_file);

    print_rlfm(rlfm);
    /* print_rlfm_s(rlfm); */
    /* print_rlfm_b(rlfm); */

    free(rlfm->S.data);
    free(rlfm->B.data);
    /* free(rlfm->Bp.data); */
    free(rlfm);
}
