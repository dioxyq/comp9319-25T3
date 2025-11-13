#include "bwt-util.h"
#include <stdio.h>

void decode(RLFM *rlfm, FILE *file) {
    size_t i = rlfm->B->end;
    unsigned char c_code = 4;
    size_t cs_c = 1;

    size_t buffer_size = KIBIBYTE;
    unsigned char *buffer = calloc(buffer_size, sizeof(unsigned char));
    size_t buffer_i = buffer_size - 1;
    size_t bytes_left = rlfm->B->len;

    for (size_t file_pos = rlfm->B->len - 1;
         (file_pos >= 0 && file_pos < 0UL - 1); --file_pos) {
        buffer[buffer_i] = ENCODING[c_code];
        if (buffer_i == 0) {
            fseek(file, file_pos, SEEK_SET);
            fwrite(buffer, sizeof(unsigned char), buffer_size, file);
            buffer_i = buffer_size - 1;
            bytes_left -= buffer_size;
        } else {
            --buffer_i;
        }

        size_t b_rank = rank_b_indexed(rlfm->B, i);
        i = select_b_indexed(rlfm->Bp,
                             cs_c + rank_s_indexed(rlfm->S, b_rank, c_code)) +
            i - select_b_indexed(rlfm->B, b_rank);

        c_code = code_from_l_pos(rlfm, i);

        cs_c = c_code == 0 ? 1 : rlfm->Cs[c_code - 1];
    }

    fseek(file, 0, SEEK_SET);
    fwrite(buffer + buffer_size - bytes_left, sizeof(unsigned char), bytes_left,
           file);
    free(buffer);
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
    RLFM *rlfm = read_rlfm(input_file);

    FILE *output_file = fopen(argv[2], "w");
    if (output_file == NULL) {
        printf("could not open output file\n");
        free_rlfm(rlfm);
        return 1;
    }
    decode(rlfm, output_file);
    fclose(output_file);

    free_rlfm(rlfm);
}
