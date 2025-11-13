#include "bwt-util.h"
#include <string.h>

void free_rank_index(RankIndex *rank_index) {
    if (rank_index == NULL) {
        return;
    }
    if (rank_index->chunk_rank != NULL) {
        free(rank_index->chunk_rank);
    }
    if (rank_index->subchunk_rank != NULL) {
        free(rank_index->subchunk_rank);
    }
    free(rank_index);
}

void free_index(Index *index) {
    if (index == NULL) {
        return;
    }
    if (index->data != NULL) {
        free(index->data);
    }
    if (index->rank_index != NULL) {
        free_rank_index(index->rank_index);
    }
    free(index);
}

void free_s_rank_index(SRankIndex *rank_index) {
    if (rank_index == NULL) {
        return;
    }
    if (rank_index->chunk_rank != NULL) {
        free(rank_index->chunk_rank);
    }
    if (rank_index->subchunk_rank != NULL) {
        free(rank_index->subchunk_rank);
    }
    free(rank_index);
}

void free_s_index(SIndex *index) {
    if (index == NULL) {
        return;
    }
    if (index->data != NULL) {
        free(index->data);
    }
    if (index->rank_index != NULL) {
        free_s_rank_index(index->rank_index);
    }
    free(index);
}

void free_rlfm(RLFM *rlfm) {
    if (rlfm == NULL) {
        return;
    }
    if (rlfm->S->data != NULL) {
        free_s_index(rlfm->S);
    }
    if (rlfm->B->data != NULL) {
        free_index(rlfm->B);
    }
    if (rlfm->Bp->data != NULL) {
        free_index(rlfm->Bp);
    }
    free(rlfm);
}

// pos must be in bounds
size_t rank_s(SIndex *s, size_t pos, unsigned char code) {
    if (pos >= s->len) {
        pos = s->len - 1;
    }
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

size_t rank_s_indexed(SIndex *s, size_t pos, unsigned char code) {
    if (code == 4) {
        return 0;
        // return pos >= s->end;
    }
    if (pos >= s->len) {
        pos = s->len - 1;
    }

    size_t subchunk_index = pos / RANK_SUBCHUNK_SIZE_BITS;
    size_t chunk_index = subchunk_index / s->rank_index->subchunks_per_chunk;

    size_t start = subchunk_index * RANK_SUBCHUNK_SIZE_BITS;
    size_t count = 0;
    for (size_t i = start / 4; i < pos / 4; ++i) {
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

    size_t relative_rank = s->rank_index->subchunk_rank[subchunk_index][code];
    size_t cumulative_rank = s->rank_index->chunk_rank[chunk_index][code];

    return cumulative_rank + relative_rank + count;
}

// count must be less than s->len
size_t select_s(SIndex *s, size_t count, unsigned char code) {
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

size_t select_s_indexed(SIndex *s, size_t count, unsigned char code) {
    // binary search on rank index subchunks
    const double COUNT_TO_POS_HEURISTIC = 4;
    const double SEARCH_WINDOW_HEURISTIC = 1;

    size_t search_window_size =
        s->len * SEARCH_WINDOW_HEURISTIC / RANK_SUBCHUNK_SIZE_BITS;
    size_t start_index =
        count * COUNT_TO_POS_HEURISTIC / RANK_SUBCHUNK_SIZE_BITS;
    size_t low = search_window_size > start_index
                     ? 0
                     : start_index - search_window_size / 2;
    size_t high = min(s->rank_index->subchunk_count - 1,
                      start_index + search_window_size / 2);
    size_t pos = low;
    size_t rank =
        s->rank_index->subchunk_rank[low][code] +
        s->rank_index
            ->chunk_rank[low / s->rank_index->subchunks_per_chunk][code];

    while (low <= high) {
        size_t mid = low + (high - low) / 2;
        size_t mid_rank =
            s->rank_index->subchunk_rank[mid][code] +
            s->rank_index
                ->chunk_rank[mid / s->rank_index->subchunks_per_chunk][code];

        if (mid_rank < count && mid_rank > rank) {
            pos = mid;
            rank = mid_rank;
        }

        if (mid_rank <= count) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }

    size_t sum = rank;
    size_t i = 2 * pos * RANK_SUBCHUNK_SIZE;
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
        count += __builtin_popcount(b->data[i]);
    }
    // count remaining bits from pos - 7 to pos
    count += __builtin_popcount((0xFF >> (7 - (pos % 8))) & b->data[pos / 8]);
    return count;
}

size_t rank_b_indexed(Index *b, size_t pos) {
    size_t subchunk_index = pos / RANK_SUBCHUNK_SIZE_BITS;
    size_t chunk_index = subchunk_index / b->rank_index->subchunks_per_chunk;

    size_t start = subchunk_index * RANK_SUBCHUNK_SIZE_BITS;
    size_t count = 0;
    for (size_t i = start / 8; i < pos / 8; i++) {
        count += __builtin_popcount(b->data[i]);
    }
    // count remaining bits from pos - 7 to pos
    count += __builtin_popcount((0xFF >> (7 - (pos % 8))) & b->data[pos / 8]);

    size_t relative_rank = b->rank_index->subchunk_rank[subchunk_index];
    size_t cumulative_rank = b->rank_index->chunk_rank[chunk_index];

    return cumulative_rank + relative_rank + count;
}

// count must be less than s->len
size_t select_b(Index *b, size_t count) {
    size_t sum = 0;
    size_t i = 0;
    for (; sum < count && i <= b->len / 8; i++) {
        sum += __builtin_popcount(b->data[i]);
    }
    --i;
    size_t pos = i * 8;
    //  correctly count overshoot byte
    if (sum >= count) {
        unsigned char byte = b->data[i];
        sum -= __builtin_popcount(byte);
        int j = 0;
        for (; sum < count; j++) {
            sum += ((1 << j) & byte) >> j;
        }
        pos += j - 1;
    }
    return pos;
}

size_t select_b_indexed(Index *b, size_t count) {
    // binary search on rank index subchunks
    const double COUNT_TO_POS_HEURISTIC = 1.5;
    const double SEARCH_WINDOW_HEURISTIC = 0.17; // 1;

    size_t search_window_size =
        b->len * SEARCH_WINDOW_HEURISTIC / RANK_SUBCHUNK_SIZE_BITS;
    size_t start_index =
        count * COUNT_TO_POS_HEURISTIC / RANK_SUBCHUNK_SIZE_BITS;
    size_t low = search_window_size > start_index
                     ? 0
                     : start_index - search_window_size / 2;
    size_t high = min(b->rank_index->subchunk_count - 1,
                      start_index + search_window_size / 2);
    size_t pos = low;
    size_t rank =
        b->rank_index->subchunk_rank[low] +
        b->rank_index->chunk_rank[low / b->rank_index->subchunks_per_chunk];

    while (low <= high) {
        size_t mid = low + (high - low) / 2;
        size_t mid_rank =
            b->rank_index->subchunk_rank[mid] +
            b->rank_index->chunk_rank[mid / b->rank_index->subchunks_per_chunk];

        if (mid_rank < count && mid_rank > rank) {
            pos = mid;
            rank = mid_rank;
        }

        if (mid_rank <= count) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }

    size_t subchunk_count = 0;
    size_t i = pos * RANK_SUBCHUNK_SIZE_BITS / 8;
    unsigned char byte = 0;
    for (; rank + subchunk_count < count; ++i) {
        byte = b->data[i];
        subchunk_count += __builtin_popcount(byte);
    }
    --i;
    subchunk_count -= __builtin_popcount(byte);

    size_t j = 0;
    for (; rank + subchunk_count < count; ++j) {
        subchunk_count += ((byte) & (1 << j)) >> j;
    }
    --j;

    return i * 8 + j;
}

unsigned char code_from_l_pos(RLFM *rlfm, size_t l_pos) {
    size_t code_pos = rank_b_indexed(rlfm->B, l_pos) - 1;
    return ((0b11 << (2 * (code_pos % 4))) & (rlfm->S->data[code_pos / 4])) >>
           (2 * (code_pos % 4));
}

void derive_rank_index(Index *index) {
    size_t n = index->len;
    size_t chunk_size_bits = RANK_SUBCHUNKS_PER_CHUNK * RANK_SUBCHUNK_SIZE_BITS;
    size_t subchunk_count =
        (n + RANK_SUBCHUNK_SIZE_BITS - 1) / RANK_SUBCHUNK_SIZE_BITS;
    size_t chunk_count =
        (n + chunk_size_bits - 1) / chunk_size_bits; // divceil n / chunk_size

    RankIndex *rank_index = malloc(sizeof(RankIndex));
    rank_index->chunk_rank =
        calloc(chunk_count, sizeof(rank_index->chunk_rank));
    rank_index->subchunk_rank =
        calloc(subchunk_count, sizeof(rank_index->subchunk_rank));
    rank_index->chunk_count = chunk_count;
    rank_index->subchunk_count = subchunk_count;
    rank_index->subchunks_per_chunk = RANK_SUBCHUNKS_PER_CHUNK;

    uint32_t cumulative_rank = 0;
    for (size_t chunk_index = 0; chunk_index < chunk_count; ++chunk_index) {
        rank_index->chunk_rank[chunk_index] = cumulative_rank;

        uint32_t relative_rank = 0;
        for (size_t subchunk_index = 0;
             subchunk_index < (chunk_index != chunk_count - 1
                                   ? RANK_SUBCHUNKS_PER_CHUNK
                                   : subchunk_count % RANK_SUBCHUNKS_PER_CHUNK);
             ++subchunk_index) {
            size_t subchunk_offset =
                chunk_index * RANK_SUBCHUNKS_PER_CHUNK + subchunk_index;

            rank_index->subchunk_rank[subchunk_offset] = relative_rank;

            uint64_t buffer = 0;
            for (int i = 0; i < RANK_SUBCHUNK_SIZE / sizeof(buffer); ++i) {
                size_t offset =
                    subchunk_offset * RANK_SUBCHUNK_SIZE + i * sizeof(buffer);
                if (offset >= index->size - sizeof(buffer)) {
                    break;
                }

                memcpy(&buffer, index->data + offset, sizeof(buffer));
                relative_rank += __builtin_popcountl(buffer);
            }
        }

        cumulative_rank += relative_rank;
    }

    index->rank_index = rank_index;
}

void derive_s_rank_index(SIndex *index) {
    size_t n = index->len;
    size_t subchunk_count =
        (n + RANK_SUBCHUNK_SIZE_BITS - 1) / RANK_SUBCHUNK_SIZE_BITS;
    size_t chunk_count =
        (n + RANK_SUBCHUNKS_PER_CHUNK * RANK_SUBCHUNK_SIZE_BITS - 1) /
        (RANK_SUBCHUNKS_PER_CHUNK *
         RANK_SUBCHUNK_SIZE_BITS); // divceil n / chunk_size

    SRankIndex *rank_index = malloc(sizeof(SRankIndex));
    rank_index->chunk_rank = calloc(chunk_count, sizeof(uint32_t[4]));
    rank_index->subchunk_rank = calloc(subchunk_count, sizeof(uint16_t[4]));
    rank_index->chunk_count = chunk_count;
    rank_index->subchunk_count = subchunk_count;
    rank_index->subchunks_per_chunk = RANK_SUBCHUNKS_PER_CHUNK;

    uint32_t cumulative_rank[4] = {0};
    size_t remaining_subchunks = subchunk_count;
    for (size_t chunk_index = 0; chunk_index < chunk_count; ++chunk_index) {
        memcpy(rank_index->chunk_rank[chunk_index], cumulative_rank,
               sizeof(uint32_t[4]));

        uint16_t relative_rank[4] = {0};
        for (size_t subchunk_index = 0;
             subchunk_index <
             min(remaining_subchunks, RANK_SUBCHUNKS_PER_CHUNK);
             ++subchunk_index) {
            size_t subchunk_offset =
                chunk_index * RANK_SUBCHUNKS_PER_CHUNK + subchunk_index;

            memcpy(rank_index->subchunk_rank[subchunk_offset], relative_rank,
                   sizeof(uint16_t[4]));

            for (int i = 0; i < 2 * RANK_SUBCHUNK_SIZE; ++i) {
                size_t offset = 2 * subchunk_offset * RANK_SUBCHUNK_SIZE + i;
                if (offset >= index->size) {
                    break;
                }
                unsigned char byte = index->data[offset];
                for (int j = 0; j < 8; j += 2) {
                    unsigned char code = (byte & (0b11 << j)) >> j;
                    ++relative_rank[code];
                }
            }
        }

        cumulative_rank[0] += relative_rank[0];
        cumulative_rank[1] += relative_rank[1];
        cumulative_rank[2] += relative_rank[2];
        cumulative_rank[3] += relative_rank[3];

        remaining_subchunks -= RANK_SUBCHUNKS_PER_CHUNK;
    }

    index->rank_index = rank_index;
}

RLFM *init_rlfm(size_t file_size) {
    size_t s_size = max(MIN_ALLOC, file_size * ST_RATIO_HEURISTIC);
    size_t b_size = max(MIN_ALLOC, file_size * BT_RATIO_HEURISTIC);
    size_t bp_size = b_size;

    RLFM *rlfm = malloc(sizeof(RLFM));

    rlfm->S = malloc(sizeof(SIndex));
    rlfm->S->data = calloc(s_size, 1);
    rlfm->S->rank_index = NULL;
    rlfm->S->size = 0;
    rlfm->S->len = 0;
    rlfm->S->end = 0;

    rlfm->B = malloc(sizeof(Index));
    rlfm->B->data = calloc(b_size, 1);
    rlfm->B->rank_index = NULL;
    rlfm->B->size = 0;
    rlfm->B->len = 0;
    rlfm->B->end = 0;

    rlfm->Bp = malloc(sizeof(Index));
    rlfm->Bp->data = NULL;
    rlfm->Bp->rank_index = NULL;
    rlfm->Bp->size = 0;
    rlfm->Bp->len = 0;
    rlfm->Bp->end = 0;

    rlfm->Cs[0] = 0;
    rlfm->Cs[1] = 0;
    rlfm->Cs[2] = 0;
    rlfm->Cs[3] = 0;

    return rlfm;
}

void read_bs(RLFM *rlfm, FILE *file, size_t file_size) {
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
                    rlfm->S->end = s_offset * 4 + s_bit_offset / 2;
                    rlfm->B->end = b_offset * 8 + b_bit_offset;
                    code = 0;
                }

                rlfm->Cs[code] += 1;

                rlfm->S->data[s_offset] |= code << s_bit_offset;
                // branchlessly update s offsets
                s_offset += (s_bit_offset == 6);
                s_bit_offset = (s_bit_offset + 2) * (s_bit_offset < 6);

                rlfm->B->data[b_offset] |= 1 << b_bit_offset;
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
        bytes_left = min(READ_BUF_SIZE, bytes_to_read);
    }

    rlfm->Cs[1] += rlfm->Cs[0];
    rlfm->Cs[2] += rlfm->Cs[1];
    rlfm->Cs[3] += rlfm->Cs[2];

    rlfm->S->size = s_offset + (s_bit_offset != 0);
    rlfm->B->size = b_offset + (b_bit_offset != 0);
    rlfm->S->len = s_offset * 4 + s_bit_offset / 2;
    rlfm->B->len = b_offset * 8 + b_bit_offset;

    rlfm->S->data = realloc(rlfm->S->data, rlfm->S->size);
    rlfm->B->data = realloc(rlfm->B->data, rlfm->B->size);

    free(buf);
}

void derive_bp(RLFM *rlfm) {
    rlfm->Bp->size = rlfm->B->size;
    rlfm->Bp->len = rlfm->B->len;
    rlfm->Bp->end = 0;
    rlfm->Bp->data = calloc(rlfm->Bp->size, 1);

    // '\n' goes at the start
    rlfm->Bp->data[0] |= 1;

    size_t bp_pos = 1;
    size_t fs_pos = 2;

    for (unsigned char code = 0; code < 4; ++code) {
        size_t curr_c_offset = fs_pos;
        for (; fs_pos <= rlfm->Cs[code]; ++fs_pos) {
            size_t s_pos =
                select_s_indexed(rlfm->S, fs_pos - curr_c_offset + 1, code);
            size_t b_pos = select_b_indexed(rlfm->B, s_pos + 1);
            rlfm->Bp->data[bp_pos / 8] |= 1 << (bp_pos % 8);

            size_t b_end = s_pos + 1 < rlfm->S->len
                               ? select_b_indexed(rlfm->B, s_pos + 2)
                               : rlfm->B->len;
            bp_pos += b_end - b_pos;
        }
    }
}

RLFM *read_rlfm(FILE *file) {
    struct stat stat_buf;
    fstat(fileno(file), &stat_buf);
    size_t file_size = stat_buf.st_size;

    RLFM *rlfm = init_rlfm(file_size);
    read_bs(rlfm, file, file_size);
    fclose(file);

    derive_s_rank_index(rlfm->S);

    derive_rank_index(rlfm->B);

    derive_bp(rlfm);

    derive_rank_index(rlfm->Bp);

    /* print_rlfm(rlfm); */
    /* print_rlfm_s(rlfm->S); */
    /* print_rlfm_b(rlfm->B); */
    /* print_rlfm_b(rlfm->Bp); */
    /* print_rank_index(rlfm->B->rank_index); */

    return rlfm;
}

void print_rank_index(RankIndex *rank_index) {
    printf(
        "chunk_count: %zu, subchunk_count: %zu, subchunks_per_chunk: %zu\n[ ",
        rank_index->chunk_count, rank_index->subchunk_count,
        rank_index->subchunks_per_chunk);
    for (size_t chunk_index = 0; chunk_index < rank_index->chunk_count;
         ++chunk_index) {
        printf("%du [ ", rank_index->chunk_rank[chunk_index]);
        for (size_t subchunk_index = 0;
             subchunk_index < rank_index->subchunk_count; ++subchunk_index) {
            size_t subchunk_offset =
                chunk_index * rank_index->subchunks_per_chunk + subchunk_index;

            printf("%du, ", rank_index->subchunk_rank[subchunk_offset]);
        }
        printf("], ");
    }
    printf("]\n");
}

void print_rlfm_s(SIndex *s) {
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
           rlfm->S->size, rlfm->B->size, rlfm->S->len, rlfm->B->len,
           rlfm->S->end, rlfm->B->end);
    printf("C: [A: %d, C: %d, G: %d, T: %d]\n", rlfm->Cs[0], rlfm->Cs[1],
           rlfm->Cs[2], rlfm->Cs[3]);
}
