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
size_t rank_s(Index *s, size_t pos, unsigned char code) {
    size_t count = 0;
    for (size_t i = 0; i < pos / 4; ++i) {
        unsigned char byte = s->data[i];
        for (int j = 0; j < 8; j += 2) {
            count += code == ((byte & (0b11 << j)) >> j);
        };
    }
    // count remaining codes from pos - 3 to pos
    unsigned char byte = s->data[pos / 4];
    for (int i = 0; i < 2 * ((pos % 4) + 1); i += 2) {
        count += code == ((byte & (0b11 << i)) >> i);
    };
    count -= (!code) * (pos >= s->end);
    return count;
}

// count must be less than s->len
size_t select_s(Index *s, size_t count, unsigned char code) {
    size_t sum = 0;
    size_t i = 0;
    int j = 0;
    int passed_end = 0;
    for (; sum < count; i++) {
        unsigned char byte = s->data[i];
        for (j = 0; j < 8; j += 2) {
            sum += code == ((byte & (0b11 << j)) >> j);
            if (sum == count) {
                // handle edge case for end symbol
                if (!passed_end && !code && (i * 4 + j / 2) >= s->end) {
                    --sum;
                    passed_end = 1;
                    continue;
                }
                break;
            }
        };
    }
    return (i - 1) * 4 + j / 2;
}

// pos must be in bounds
size_t rank_b(Index *b, size_t pos) {
    size_t count = 0;
    for (size_t i = 0; i < pos / 8; i++) {
        count += COUNT_ONES[b->data[i]];
    }
    // count remaining bits from pos - 7 to pos
    count += COUNT_ONES[(0xFF >> (7 - (pos % 8))) & b->data[pos / 8]];
    return count;
}

// count must be less than s->len
size_t select_b(Index *b, size_t count) {
    size_t sum = 0;
    size_t i = 0;
    for (; sum < count && i <= b->len / 8; i++) {
        sum += COUNT_ONES[b->data[i]];
    }
    --i;
    size_t pos = i * 8;
    //  correctly count overshoot byte
    if (sum >= count) {
        unsigned char byte = b->data[i];
        sum -= COUNT_ONES[byte];
        int j = 0;
        for (; sum < count; j++) {
            sum += ((1 << j) & byte) >> j;
        }
        pos += j - 1;
    }
    return pos;
}

void print_rlfm_s(Index *s) {
    for (size_t i = 0; i < (s->len + 3) / 4; ++i) {
        char byte = s->data[i];
        char buf[5] = {'\0'};
        for (int j = 0; j < min(4, s->len - i * 4); ++j) {
            char code = (byte & (0b11 << (2 * j))) >> (2 * j);
            if (i * 4 + j == s->end) {
                code = 4;
            }
            buf[j] = ENCODING[code];
        }
        printf("%s", buf);
    }
    printf("\n");
}

void print_rlfm_b(Index *b) {
    for (size_t i = 0; i < (b->len + 7) / 8; ++i) {
        char byte = b->data[i];
        for (int j = 0; j < min(8, b->len - i * 8); ++j) {
            char bit = (byte & (1 << j)) >> j;
            printf("%d", bit);
        }
    }
    printf("\n");
}

void print_rlfm(RLFM *rlfm) {
    printf("s_size: %zu, b_size: %zu, s_len: %zu, b_len: %zu, s_end: %zu, "
           "b_end: %zu, ",
           rlfm->S.size, rlfm->B.size, rlfm->S.len, rlfm->B.len, rlfm->S.end,
           rlfm->B.end);
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

    rlfm->S.size = s_offset + (s_bit_offset != 0);
    rlfm->B.size = b_offset + (b_bit_offset != 0);
    rlfm->S.len = s_offset * 4 + s_bit_offset / 2;
    rlfm->B.len = b_offset * 8 + b_bit_offset;

    rlfm->S.data = realloc(rlfm->S.data, rlfm->S.size);
    rlfm->B.data = realloc(rlfm->B.data, rlfm->B.size);

    free(buf);
}

void derive_bp(RLFM *rlfm) {
    rlfm->Bp.size = rlfm->B.size;
    rlfm->Bp.len = rlfm->B.len;
    rlfm->Bp.end = 0;
    rlfm->Bp.data = calloc(rlfm->Bp.size, 1);

    // '\n' goes at the start
    rlfm->Bp.data[0] |= 1;

    size_t bp_pos = 1;
    size_t fs_pos = 1;

    for (unsigned char code = 0; code < 4; ++code) {
        size_t curr_c_offset = fs_pos;
        for (; fs_pos <= rlfm->Cs[code]; ++fs_pos) {
            if (fs_pos == rlfm->S.end) {
                continue;
            }
            size_t s_pos = select_s(&rlfm->S, fs_pos - curr_c_offset + 1, code);
            size_t b_pos = select_b(&rlfm->B, s_pos + 1);
            rlfm->Bp.data[bp_pos / 8] |= 1 << (bp_pos % 8);

            size_t b_end = select_b(&rlfm->B, s_pos + 2);
            bp_pos += b_end - b_pos;
        }
    }
}

void decode_out(RLFM *rlfm, FILE *file) {
    size_t i = rlfm->B.end;
    unsigned char c_code = 4;
    size_t cs_c = 1;

    for (size_t file_pos = rlfm->B.len - 1;
         (file_pos >= 0 && file_pos < 0UL - 1); --file_pos) {
        fseek(file, file_pos, SEEK_SET);
        fputc(ENCODING[c_code], file);

        i = select_b(&rlfm->Bp,
                     cs_c + rank_s(&rlfm->S, rank_b(&rlfm->B, i), c_code)) +
            i - select_b(&rlfm->B, rank_b(&rlfm->B, i));

        size_t code_pos = rank_b(&rlfm->B, i) - 1;
        c_code =
            ((0b11 << (2 * (code_pos % 4))) & (rlfm->S.data[code_pos / 4])) >>
            (2 * (code_pos % 4));

        cs_c = c_code == 0 ? 1 : rlfm->Cs[c_code - 1];
    }
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

    /* print_rlfm(rlfm); */
    /* print_rlfm_s(&rlfm->S); */
    /* print_rlfm_b(&rlfm->B); */
    /* print_rlfm_b(&rlfm->Bp); */

    free(rlfm->S.data);
    free(rlfm->B.data);
    free(rlfm->Bp.data);
    free(rlfm);
}
