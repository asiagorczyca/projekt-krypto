import gmpy2
import sys
import random 


sys.set_int_max_str_digits(10000)


def generate_prime(bits: int) -> int:
    
    seed = random.randint(0, 10**20)
    rs = gmpy2.random_state(seed)

    candidate = gmpy2.mpz_urandomb(rs, bits)
    

    candidate = gmpy2.bit_set(candidate, bits - 1)
    candidate = gmpy2.bit_set(candidate, 0)
    
    p = gmpy2.next_prime(candidate)
    return int(p)

def find_generator(p: int) -> int:
    p_mpz = gmpy2.mpz(p)

    if gmpy2.is_even(p_mpz): return 2

    
    rs = gmpy2.random_state()
    p_minus_1_div_2 = (p_mpz - 1) // 2
    
    while True:
        g = gmpy2.mpz_random(rs, p_mpz - 3) + 2
        if gmpy2.powmod(g, 2, p_mpz) == 1: 
            continue
        if gmpy2.powmod(g, p_minus_1_div_2, p_mpz) == 1: 
            continue
            
        return int(g)


def generate_private(p: int) -> int:
    rs = gmpy2.random_state()
    priv = gmpy2.mpz_random(rs, gmpy2.mpz(p) - 3) + 2
    return int(priv)

def compute_public(g: int, priv: int, p: int) -> int:
    res = gmpy2.powmod(gmpy2.mpz(g), gmpy2.mpz(priv), gmpy2.mpz(p))
    return int(res)

def compute_shared(priv: int, peer_pub: int, p: int) -> int:
    res = gmpy2.powmod(gmpy2.mpz(peer_pub), gmpy2.mpz(priv), gmpy2.mpz(p))
    return int(res)