import sys

encoding = {"A": 0, "C": 1, "G": 2, "T": 3, "\n": 4}

def main():
    file_name = sys.argv[1]
    output_file_name = sys.argv[2]

    data = bytearray()

    with open(file_name, "r") as file:
        prev = file.read(1)
        run_length = 1

        while char := file.read(1):
            if char != prev or run_length == 32:
                data.append((run_length - 1) | (encoding[prev] << 5))
                prev = char
                run_length = 1
            else:
                run_length += 1
        data.append((run_length - 1) | (encoding[prev] << 5))

    with open(output_file_name, "wb") as file:
        file.write(data)


if __name__ == "__main__":
    main()
