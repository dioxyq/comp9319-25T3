import sys

def main():
    file_name = sys.argv[1]

    count = 0
    sum = 0

    with open(file_name, "rb") as file:
        while byte := file.read(1):
            count += 1
            run_length = (byte[0] & 31) + 1
            sum += run_length

    avg = sum / count
            
    print(avg)


if __name__ == "__main__":
    main()
