import argparse
import random
#from podstawa_krypto.df_imp.generowanie_liczb_losowych import randomint,randomint_for_dh

def is_probable_prime(n: int, rounds: int = 20) -> bool:
    if n < 2:
        return False
    small_primes = (2,3,5,7,11,13,17,19,23,29)
    for p in small_primes:
        if n % p == 0:
            return n == p

    d = n - 1
    s = 0
    while d % 2 == 0:
        d //= 2
        s += 1

    for _ in range(rounds):
        a = random.randint(2, n - 2)
        x = pow(a, d, n)
        if x == 1 or x == n - 1:
            continue
        for _ in range(s - 1):
            x = pow(x, 2, n)
            if x == n - 1:
                break
        else:
            return False
    return True

def generate_random_odd(bits: int) -> int:
    x = random.randint(2**(bits-1), 2**bits - 1)
    if x % 2 == 0:
        x += 1
    return x

def generate_prime(bits: int, rounds: int = 20) -> int:
    while True:
        p = generate_random_odd(bits)
        if is_probable_prime(p, rounds=rounds):
            return p

def find_generator(p: int) -> int:
    for _ in range(1000):
        g = random.randint(2, p - 2)
        if pow(g, (p-1)//2, p) != 1:  
            return g
    return 2  


def generate_private(p: int) -> int:
    return random.randint(2, p - 2)

# change into one funkction 

def compute_public(g: int, priv: int, p: int) -> int:
    return pow(g, priv, p)

def compute_shared(priv: int, peer_pub: int, p: int) -> int:
    return pow(peer_pub, priv, p)

def dh_demo(bits: int, rounds: int = 20):
    print(f"[INFO] Generacja p ({bits} bitów)...")
    p = generate_prime(bits, rounds=rounds)
    print(f"Wygenerowano p: {p}")

    g = find_generator(p)
    print(f"Generator g: {g}")

    # klucze Alice i Bob
    a_priv = generate_private(p)
    b_priv = generate_private(p)
    A_pub = compute_public(g, a_priv, p)
    B_pub = compute_public(g, b_priv, p)

    print(f"\nPublic Alice: {A_pub}")
    print(f"Public Bob:   {B_pub}")

    S_a = compute_shared(a_priv, B_pub, p)
    S_b = compute_shared(b_priv, A_pub, p)

    print(f"\nSekret Alice: {S_a}")
    print(f"Sekret Bob:   {S_b}")
    print(f"Sekrety się zgadzają? {'Tak' if S_a == S_b else 'Nie'}")

def main():
    parser = argparse.ArgumentParser(description="Skrócona demonstracja Diffie-Hellmana")
    parser.add_argument("--bits", type=int, default=512,
                        help="Długość bitowa liczby pierwszej p (domyślnie 512 dla szybszych testów)")
    parser.add_argument("--rounds", type=int, default=20,
                        help="Liczba rund testu Millera-Rabina (domyślnie 20)")
    args = parser.parse_args()

    dh_demo(bits=args.bits, rounds=args.rounds)


if __name__ == "__main__":
    main()