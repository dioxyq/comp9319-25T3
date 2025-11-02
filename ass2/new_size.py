import sys

def main():
    file_name = sys.argv[1]

    sum = 0

    with open(file_name, "rb") as file:
        while byte := file.read(1):
            run_length = (byte[0] & 31) + 1
            sum += 3 * -(run_length // -2)

    mega_bytes = (sum / 8) / (10**6)

    print(mega_bytes)


if __name__ == "__main__":
    main()
