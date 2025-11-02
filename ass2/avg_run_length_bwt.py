import sys

def main():
    file_name = sys.argv[1]

    count = 0
    sum = 0

    with open(file_name, "r") as file:
        run_length = 1
        last = '\n'
        while char := file.read(1):
            if (char == last):
                run_length += 1
            else:
                sum += run_length
                run_length = 1
                count += 1
            last = char

    avg = sum / count
            
    print(avg)


if __name__ == "__main__":
    main()
