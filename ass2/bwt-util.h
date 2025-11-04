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

void free_rlfm(RLFM *rlfm);
// pos must be in bounds
size_t rank_s(Index *s, size_t pos, unsigned char code);
// count must be less than total number of code
size_t select_s(Index *s, size_t count, unsigned char code);
// pos must be in bounds
size_t rank_b(Index *b, size_t pos);
// count must be less than total number of 1s
size_t select_b(Index *b, size_t count);
size_t code_from_l_pos(RLFM *rlfm, size_t l_pos);
RLFM *init_rlfm(size_t file_size);
void read_bs(RLFM *rlfm, FILE *file, size_t file_size);
void derive_bp(RLFM *rlfm);
RLFM *read_rlfm(FILE *file);
void print_rlfm_s(Index *s);
void print_rlfm_b(Index *b);
void print_rlfm(RLFM *rlfm);
