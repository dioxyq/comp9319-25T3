import sys
import math
from collections import Counter

def entropy(s: str) -> float:
    return sum(map(lambda c : -(c / len(s)) * math.log2(c / len(s)), Counter(s).values()))

if __name__ == '__main__':
    print(entropy(sys.argv[1]))
