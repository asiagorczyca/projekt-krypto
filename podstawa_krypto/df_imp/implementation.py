import multiprocessing
import random
import sys
import time
import os
from liczby_losowe.our_own_csrng import KeccakBasedCSPRNG
try:
    import gmpy2
    from gmpy2 import mpz
except ImportError:
    print("Error: gmpy2 is missing. Please run: pip install gmpy2")
    sys.exit(1)

sys.set_int_max_str_digits(10000)

SEARCH_BATCH_SIZE = 50000  
MR_ROUNDS = 40             

RFC_3526_PRIME_4096_HEX = (
    "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1"
    "29024E088A67CC74020BBEA63B139B22514A08798E3404DD"
    "EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245"
    "E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED"
    "EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D"
    "C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F"
    "83655D23DCA3AD961C62F356208552BB9ED529077096966D"
    "670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B"
    "E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9"
    "DE2BCBF6955817183995497CEA956AE515D2261898FA0510"
    "15728E5A8AACAA68FFFFFFFFFFFFFFFFC90FDAA22168C234"
    "C4C6628B80DC1CD129024E088A67CC74020BBEA63B139B22"
    "514A08798E3404DDEF9519B3CD3A431B302B0A6DF25F1437"
    "4FE1356D6D51C245E485B576625E7EC6F44C42E9A637ED6B"
    "0BFF5CB6F406B7EDEE386BFB5A899FA5AE9F24117C4B1FE6"
    "49286651ECE45B3DC2007CB8A163BF0598DA48361C55D39A"
    "69163FA8FD24CF5F83655D23DCA3AD961C62F356208552BB"
    "9ED529077096966D670C354E4ABC9804F1746C08CA18217C"
    "32905E462E36CE3BE39E772C180E86039B2783A2EC07A28F"
    "B5C55DF06F4C52C9DE2BCBF6955817183995497CEA956AE5"
    "15D2261898FA051015728E5A8AAAC42DAD33170D04507A33"
    "A85521ABDF1CBA64ECFB850458DBEF0A8AEA71575D060C7D"
    "B3970F85A6E1E4C7ABF5AE8CDB0933D71E8C94E04A25619D"
    "CEE3D2261AD2EE6BF12FFA06D98A0864D87602733EC86A64"
    "521F2B18177B200CBBE117577A615D6C770988C0BAD946E2"
    "08E24FA074E5AB3143DB5BFCE0FD108E4B82D120A93AD2CA"
    "FFFFFFFFFFFFFFFF"
)

def get_primorial(n):
    res = mpz(1)
    p = 2
    while p <= n:
        res *= p
        p = gmpy2.next_prime(p)
    return res


PRIMORIAL_2000 = get_primorial(2000)

def get_standard_parameters():
    return int(RFC_3526_PRIME_4096_HEX, 16), 2

def find_generator(p: int) -> int:
    p_mpz = mpz(p)
    q_mpz = (p_mpz - 1) // 2
    
    rs = gmpy2.random_state(random.SystemRandom().getrandbits(64))
    
    while True:
        g = gmpy2.mpz_random(rs, p_mpz - 3) + 2
        
        if gmpy2.powmod(g, 2, p_mpz) == 1: continue
        if gmpy2.powmod(g, q_mpz, p_mpz) == 1: continue
            
        return int(g)

def safe_prime_worker(bits, stop_event, result_queue):
    min_q = mpz(2)**(bits - 2)
    
    local_primorial = PRIMORIAL_2000
    
    while not stop_event.is_set():
        seed_bytes = os.urandom(bits // 8)
        q_candidate = mpz(int.from_bytes(seed_bytes, 'big')) | min_q
        
        if q_candidate % 2 == 0:
            q_candidate += 1
            
        curr = q_candidate

        for _ in range(SEARCH_BATCH_SIZE):
            if stop_event.is_set():
                return

            if gmpy2.gcd(curr, local_primorial) != 1:
                curr += 2
                continue
                

            p = 2 * curr + 1
            if gmpy2.gcd(p, local_primorial) != 1:
                curr += 2
                continue


            if gmpy2.is_prime(p, 1):

                if gmpy2.is_prime(curr, 1):
                    if gmpy2.is_prime(p, MR_ROUNDS) and gmpy2.is_prime(curr, MR_ROUNDS):
                        stop_event.set()
                        result_queue.put((int(p), int(curr)))
                        return
            
            curr += 2

def get_parameters_parallel(bits, timeout=10):
    num_workers = multiprocessing.cpu_count()
    if num_workers > 6: num_workers = 6
        
    print(f"Generating {bits}-bit Safe Prime using {num_workers} workers...")
    
    result_queue = multiprocessing.Queue()
    stop_event = multiprocessing.Event()
    processes = []
    
    
    for _ in range(num_workers):
        p = multiprocessing.Process(
            target=safe_prime_worker, 
            args=(bits, stop_event, result_queue)
        )
        p.daemon = True
        p.start()
        processes.append(p)
        
    p_val = None
    g_val = None
    
    try:
        p_found, _ = result_queue.get(timeout=timeout)
        
        p_val = p_found
        g_val = find_generator(p_val)
        
    except multiprocessing.queues.Empty:
        print(f"Timeout reached. Switching to RFC 3526 Standard Parameters.")
        p_val, g_val = get_standard_parameters()
    finally:
        stop_event.set()
        for p in processes:
            p.terminate()
            p.join()
            
    return p_val, g_val

def generate_private(p: int) -> int:
    rng = KeccakBasedCSPRNG(
        dir_for_files=None,  
        reseed_interval_calls=3000
    )
    return rng.get_random_range(a=2,b=p-2)
    #return random.SystemRandom().randint(2, p - 2)

def compute_public(g: int, priv: int, p: int) -> int:
    return int(gmpy2.powmod(gmpy2.mpz(g), gmpy2.mpz(priv), gmpy2.mpz(p)))

def compute_shared(priv: int, peer_pub: int, p: int) -> int:
    return int(gmpy2.powmod(gmpy2.mpz(peer_pub), gmpy2.mpz(priv), gmpy2.mpz(p)))