import sys
import ast
from collections import Counter
from functools import reduce


class ArithmeticEncoding:
    value: float
    size: int
    table: list[tuple[str, float]]

    def __init__(self, value: float, size: int, table: list[tuple[str, float]]):
        self.value = value
        self.size = size
        self.table = table

    def __str__(self):
        return f"value: {self.value}, size: {self.size}, table: {self.table}"

    def get_bounds(self, char: str) -> tuple[float, float]:
        lower = 0
        for c, upper in self.table:
            if c == char:
                return lower, upper
            lower = upper
        raise Exception("bad encoding table")

    def get_char(self, index: float) -> str:
        for char, upper in self.table:
            if upper > index:
                return char
        raise Exception("bad encoding table")

    @classmethod
    def encode(cls, s: str):
        lower = 0
        table = [(char, lower := lower + count / len(s)) for char, count in sorted(Counter(s).items())]
        ae = ArithmeticEncoding(0, len(s), table)

        mul = 1
        for c in s:
            lower, upper = ae.get_bounds(c)
            ae.value += lower * mul 
            mul *= upper - lower

        return ae

    def decode(self) -> str:
        res = ''
        i = self.value
        for _ in range(0, self.size):
            new_char = self.get_char(i)
            lower, upper = self.get_bounds(new_char)
            i = (i - lower) / (upper - lower)
            res += new_char
        return res


def main():
    if sys.argv[1] == '--decode':
        print(ArithmeticEncoding(float(sys.argv[2]), int(sys.argv[3]), ast.literal_eval(sys.argv[4])).decode())
    else:
        print(ArithmeticEncoding.encode(sys.argv[1]))


if __name__ == '__main__':
    main()
