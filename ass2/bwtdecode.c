#include "bwt-util.h"

void decode(RLFM *rlfm, FILE *file) {
    size_t i = rlfm->B->end;
    unsigned char c_code = 4;
    size_t cs_c = 1;

    for (size_t file_pos = rlfm->B->len - 1;
         (file_pos >= 0 && file_pos < 0UL - 1); --file_pos) {
        fseek(file, file_pos, SEEK_SET);
        fputc(ENCODING[c_code], file);

        i = select_b_indexed(rlfm->Bp,
                             cs_c + rank_s_indexed(rlfm->S,
                                                   rank_b_indexed(rlfm->B, i),
                                                   c_code)) +
            i - select_b_indexed(rlfm->B, rank_b_indexed(rlfm->B, i));

        c_code = code_from_l_pos(rlfm, i);

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
    RLFM *rlfm = read_rlfm(input_file);

    FILE *output_file = fopen(argv[2], "w");
    decode(rlfm, output_file);
    fclose(output_file);

    /* print_rlfm(rlfm); */
    /* print_rlfm_s(&rlfm->S); */
    /* print_rlfm_b(&rlfm->B); */
    /* print_rlfm_b(&rlfm->Bp); */
    /* print_rank_index(rlfm->B.rank_index); */

    free_rlfm(rlfm);
}
