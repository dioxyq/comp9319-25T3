#include <math.h>
#include <stdint.h>
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
static const size_t RANK_SUBCHUNK_SIZE_BITS = 256;
static const size_t RANK_SUBCHUNK_SIZE = 32;
static const size_t RANK_SUBCHUNKS_PER_CHUNK = 256;

typedef struct {
    uint32_t *chunk_rank;    // n/log(n) < 27 < 32 for n=110x10^6
    uint16_t *subchunk_rank; // log^2(n) / 64 < 10 < 16 for n=110x10^6
    size_t chunk_count;
    size_t subchunk_count;
    size_t subchunks_per_chunk;
} RankIndex; // Jacobson's rank

typedef struct {
    unsigned char *data;
    RankIndex *rank_index;
    size_t size;
    size_t len;
    size_t end;
} Index;

typedef struct {
    uint32_t (*chunk_rank)[4];
    uint16_t (*subchunk_rank)[4];
    size_t chunk_count;
    size_t subchunk_count;
    size_t subchunks_per_chunk;
} SRankIndex;

typedef struct {
    unsigned char *data;
    SRankIndex *rank_index;
    size_t size;
    size_t len;
    size_t end;
} SIndex;

static const Index NEW_INDEX = {
    .data = NULL, .rank_index = NULL, .size = 0, .len = 0, .end = 0};

static const SIndex NEW_SINDEX = {
    .data = NULL, .rank_index = NULL, .size = 0, .len = 0, .end = 0};

typedef struct {
    SIndex *S;
    Index *B;
    Index *Bp;
    unsigned int Cs[4];
} RLFM;

void free_rank_index(RankIndex *jacobson_rank);
void free_rlfm(RLFM *rlfm);
// pos must be in bounds
size_t rank_s(SIndex *s, size_t pos, unsigned char code);
// count must be less than total number of code
size_t select_s(SIndex *s, size_t count, unsigned char code);
// pos must be in bounds
size_t rank_b(Index *b, size_t pos);
size_t rank_b_indexed(Index *b, size_t pos);
// count must be less than total number of 1s
size_t select_b(Index *b, size_t count);
size_t select_b_indexed(Index *b, size_t count);
unsigned char code_from_l_pos(RLFM *rlfm, size_t l_pos);
RankIndex *init_rank_index(size_t chunk_count, size_t subchunk_count);
void derive_rank_index(Index *index);
void derive_s_rank_index(SIndex *index, unsigned int Cs[4]);
RLFM *init_rlfm(size_t file_size);
void read_bs(RLFM *rlfm, FILE *file, size_t file_size);
void derive_bp(RLFM *rlfm);
// closes file
RLFM *read_rlfm(FILE *file);
void print_rank_index(RankIndex *rank_index);
void print_rlfm_s(SIndex *s);
void print_rlfm_b(Index *b);
void print_rlfm(RLFM *rlfm);
