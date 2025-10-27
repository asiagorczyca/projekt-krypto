import random

sr = random.SystemRandom()
count = 100000

with open("unsafe16.bin", "wb") as f:
    for _ in range(count):
        n = sr.getrandbits(16)
        n |= (1 << 2047)
        f.write(n.to_bytes(256, 'big'))
