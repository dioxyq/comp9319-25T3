import sys

def main():
    file_name = sys.argv[1]

    chars = ['A', 'C', 'G', 'T']

    orig_byte_sum = 0
    b_bit_sum = 0
    s_bit_sum = 0

    with open(file_name, "rb") as file:
        last = '\n'
        while byte := file.read(1):
            orig_byte_sum += 1

            run_length = (byte[0] & 0b00011111) + 1
            b_bit_sum += run_length

            char = chars[((byte[0] & 0b01100000) >> 5)]
            if (char != last):
                s_bit_sum += 2
                last = char

    b_mb = (b_bit_sum / 8) / (10**6)
    s_mb = (s_bit_sum / 8) / (10**6)
    total_mb = b_mb + s_mb
    orig_mb = orig_byte_sum / (10**6)
    compression_ratio = orig_mb / total_mb

    print("Original size (MB): ", orig_mb)
    print("B (MB): ", b_mb)
    print("S (MB): ", s_mb)
    print("Total (MB): ", total_mb)
    print("Compression ratio: ", compression_ratio)


if __name__ == "__main__":
    main()
