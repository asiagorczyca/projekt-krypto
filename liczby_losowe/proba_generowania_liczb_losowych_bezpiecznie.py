
"""
keccak_rng_no_secrets.py

Keccak-based CSPRNG bez użycia biblioteki `secrets`.
Źródła entropii:
 - hashowanie (SHA3/Keccak) plików zmienionych w ostatnim X minutach,
 - 50 IV (podane lub generowane przez os.urandom),
 - czas / PID / os.urandom rezerwa,
 - opcjonalnie ruch sieciowy (psutil),
Rdzeń: HMAC-DRBG z HMAC-SHA3-256.

CLI:
  python keccak_rng_no_secrets.py --bytes 32 --dir /path/to/files
  python keccak_rng_no_secrets.py --bits 256 --dir /path/to/files
"""

import os
import time
import hashlib
import hmac
import struct
import random
import argparse
from typing import Iterable, List, Optional

# opcjonalnie psutil (ruch sieciowy)
try:
    import psutil
    _have_psutil = True
except Exception:
    _have_psutil = False


# ---------------------------
# HMAC-DRBG (SHA3-256)
# ---------------------------
class HMAC_DRBG_SHA3:
    """
    Prosta implementacja HMAC-DRBG używająca SHA3-256 jako funkcji HMAC.
    """

    def __init__(self, seed: bytes):
        self.K = b'\x00' * 32
        self.V = b'\x01' * 32
        self._hmac = lambda key, data: hmac.new(key, data, hashlib.sha3_256).digest()
        self.reseed(seed)

    def reseed(self, seed_material: bytes):
        self.K = self._hmac(self.K, self.V + b'\x00' + seed_material)
        self.V = self._hmac(self.K, self.V)
        if seed_material:
            self.K = self._hmac(self.K, self.V + b'\x01' + seed_material)
            self.V = self._hmac(self.K, self.V)

    def generate(self, nbytes: int) -> bytes:
        out = b''
        while len(out) < nbytes:
            self.V = self._hmac(self.K, self.V)
            out += self.V
        return out[:nbytes]


# ---------------------------
# Funkcje zbierania entropii
# ---------------------------
def sha3_512_bytes(data: bytes) -> bytes:
    return hashlib.sha3_512(data).digest()

def collect_mouse_entropy(samples: int = 16) -> bytes:
    try:
        import pyautogui
        import time
        m = hashlib.sha3_512()
        for _ in range(samples):
            x, y = pyautogui.position()
            m.update(struct.pack('<II', x, y))
            m.update(struct.pack('<d', time.time()))
            time.sleep(0.01)
        return m.digest()
    except Exception:
        return b''

# 2. Entropia ze zrzutu ekranu
def collect_entropy_from_screenshot(samples: int = 1024) -> bytes:
    try:
        from PIL import ImageGrab
        import random
        img = ImageGrab.grab()
        img = img.convert('RGB')
        w, h = img.size
        pixels = []
        rnd = random.Random()
        for _ in range(samples):
            px = img.getpixel((rnd.randrange(w), rnd.randrange(h)))
            pixels.append(px)
        m = hashlib.sha3_512()
        for px in pixels:
            m.update(struct.pack('<BBB', *px))
        return m.digest()
    except Exception:
        return b''

# 3. Entropia z parametrów sprzętowych
def collect_entropy_from_hardware() -> bytes:
    m = hashlib.sha3_512()
    try:
        import psutil
        # CPU
        m.update(struct.pack('<d', time.time()))
        for perc in psutil.cpu_percent(percpu=True):
            m.update(struct.pack('<f', perc))
        m.update(struct.pack('<d', sum(psutil.cpu_times().user for _ in range(1))))
        # RAM
        mem = psutil.virtual_memory()
        m.update(struct.pack('<Q', mem.available))
        m.update(struct.pack('<Q', mem.used))
        # Temperatura jeśli dostępna
        if hasattr(psutil, "sensors_temperatures"):
            temps = psutil.sensors_temperatures()
            for k, v in temps.items():
                for entry in v:
                    m.update(struct.pack('<f', entry.current))
        # GPU (jeśli GPUtil dostępne)
        try:
            import GPUtil
            gpus = GPUtil.getGPUs()
            for gpu in gpus:
                m.update(struct.pack('<f', gpu.load))
                m.update(struct.pack('<f', gpu.memoryUtil))
                m.update(struct.pack('<f', gpu.temperature))
        except Exception:
            pass
    except Exception:
        pass
    return m.digest()

def collect_file_entropy(dirpath: str,
                         timeframe_minutes: int = 10,
                         samples: int = 8,
                         max_read_bytes: int = 16384) -> bytes:
    """
    Wybiera pliki z katalogu zmienione w ostatnim `timeframe_minutes`.
    Losowo wybiera `samples` plików (jeśli dostępne) i hashuje ich fragmenty.
    Zwraca SHA3-512 z concatenacji.
    """
    if not dirpath or not os.path.isdir(dirpath):
        return b''

    now = time.time()
    cutoff = now - (timeframe_minutes * 60)
    filepaths = []
    for root, dirs, files in os.walk(dirpath):
        for f in files:
            p = os.path.join(root, f)
            try:
                if os.path.getmtime(p) >= cutoff:
                    filepaths.append(p)
            except Exception:
                continue

    if not filepaths:
        # jeśli brak plików w timeframe, weź kilka ostatnio zmienionych
        for root, dirs, files in os.walk(dirpath):
            for f in files:
                filepaths.append(os.path.join(root, f))
        if not filepaths:
            return b''

    m = hashlib.sha3_512()
    rnd = random.SystemRandom()
    chosen = rnd.sample(filepaths, min(samples, len(filepaths)))

    for p in chosen:
        try:
            st = os.stat(p)
            m.update(p.encode('utf-8', errors='ignore'))
            m.update(struct.pack('<d', float(st.st_mtime)))
            with open(p, 'rb') as fh:
                head = fh.read(min(4096, max_read_bytes))
                try:
                    fh.seek(-min(4096, max_read_bytes), os.SEEK_END)
                    tail = fh.read(min(4096, max_read_bytes))
                except Exception:
                    tail = b''
                combined = head + b'--' + tail
                m.update(hashlib.sha3_512(combined).digest())
        except Exception:
            continue

    return m.digest()


def collect_network_entropy() -> bytes:
    """
    Zbiera proste statystyki sieciowe (jeśli psutil dostępne).
    """
    if not _have_psutil:
        return b''
    try:
        s0 = psutil.net_io_counters()
        time.sleep(0.05)
        s1 = psutil.net_io_counters()
        m = hashlib.sha3_512()
        fields = ['bytes_sent', 'bytes_recv', 'packets_sent', 'packets_recv']
        for f in fields:
            v0 = getattr(s0, f, 0)
            v1 = getattr(s1, f, 0)
            m.update(struct.pack('<Q', int(v0)))
            m.update(struct.pack('<Q', int(v1)))
            m.update(struct.pack('<Q', int(abs(v1 - v0))))
        m.update(struct.pack('<d', time.time()))
        return m.digest()
    except Exception:
        return b''


def collect_time_entropy() -> bytes:
    m = hashlib.sha3_512()
    m.update(struct.pack('<d', time.time()))
    m.update(struct.pack('<I', os.getpid()))
    # dołącz losowe 16B z systemu jako rezerwa
    m.update(os.urandom(16))
    return m.digest()


def prepare_iv_list(iv_iter: Optional[Iterable[bytes]], count: int = 50) -> List[bytes]:
    """
    Przygotuj listę `count` IV. Jeśli iv_iter podane -> użyj (trunc/triple) do 32B,
    inaczej wygeneruj z os.urandom.
    """
    out = []
    if iv_iter:
        for iv in iv_iter:
            if isinstance(iv, str):
                iv = iv.encode('utf-8', errors='ignore')
            out.append(hashlib.sha3_256(iv).digest())
            if len(out) >= count:
                break
    while len(out) < count:
        out.append(hashlib.sha3_256(os.urandom(32)).digest())
    return out[:count]


# ---------------------------
# Budowanie seeda / miksowanie
# ---------------------------
def build_seed(dir_for_files: Optional[str],
               ivs: Optional[Iterable[bytes]],
               timeframe_minutes: int = 10) -> bytes:
    """
    Zbierz wszystkie źródła entropii, pomieszaj przez SHA3-512 i zwróć 32-bajtowy seed.
    """
    pieces = []
    pieces.append(hashlib.sha3_512(os.urandom(64)).digest())
    pieces.append(collect_time_entropy())
    pieces.append(collect_file_entropy(dir_for_files, timeframe_minutes=timeframe_minutes, samples=10))
    pieces.append(collect_network_entropy())
    pieces.append(collect_mouse_entropy())
    pieces.append(collect_entropy_from_screenshot())
    pieces.append(collect_entropy_from_hardware())
    
    iv_list = prepare_iv_list(ivs, count=50)
    for iv in iv_list:
        pieces.append(iv)

    h = hashlib.sha3_512()
    for p in pieces:
        if not p:
            continue
        h.update(p)
    big = h.digest()
    seed32 = hashlib.sha3_256(big).digest()
    return seed32


# ---------------------------
# Publiczny CSPRNG
# ---------------------------
class KeccakBasedCSPRNG:
    def __init__(self,
                 dir_for_files: Optional[str] = None,
                 ivs: Optional[Iterable[bytes]] = None,
                 timeframe_minutes: int = 10,
                 reseed_interval_calls: int = 1024):
        self.dir_for_files = dir_for_files
        self.ivs = ivs
        self.timeframe_minutes = timeframe_minutes
        self._reseed_interval = int(reseed_interval_calls)
        seed = build_seed(dir_for_files, ivs, timeframe_minutes)
        self._drbg = HMAC_DRBG_SHA3(seed)
        self._calls = 0

    def reseed(self):
        seed = build_seed(self.dir_for_files, self.ivs, self.timeframe_minutes)
        self._drbg.reseed(seed)
        self._calls = 0

    def get_random_bytes(self, n: int) -> bytes:
        if self._calls and (self._calls % self._reseed_interval == 0):
            self.reseed()
        self._calls += 1
        return self._drbg.generate(n)

    def get_random_int(self, nbits: int) -> int:
        if nbits <= 0:
            return 0
        nbytes = (nbits + 7) // 8
        while True:
            b = self.get_random_bytes(nbytes)
            val = int.from_bytes(b, 'big')
            excess = (1 << (8 * nbytes)) % (1 << nbits)
            limit = (1 << (8 * nbytes)) - excess
            if val < limit:
                return val & ((1 << nbits) - 1)

    def get_random_hex(self, nbytes: int) -> str:
        return self.get_random_bytes(nbytes).hex()


# ---------------------------
# CLI
# ---------------------------
def main():
    parser = argparse.ArgumentParser(description="Keccak-based CSPRNG mixing file-hashes, IVs, time and network stats (no secrets lib)")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--bytes", type=int, help="Ilość bajtów losowych do wygenerowania")
    group.add_argument("--bits", type=int, help="Ilość bitów losowych do wygenerowania (zwrócone jako integer decimal)")
    parser.add_argument("--dir", type=str, default=None, help="Katalog plików do hashowania (opcjonalne)")
    parser.add_argument("--timeframe", type=int, default=10, help="Okres modyfikacji plików w minutach (domyślnie 10)")
    parser.add_argument("--reseed", type=int, default=1024, help="Re-seed co N wywołań (domyślnie 1024)")
    parser.add_argument("--no-network", action="store_true", help="Wyłącz zbieranie statystyk sieciowych")
    parser.add_argument("--ivs-file", type=str, default=None, help="Plik z liniami IV (opcjonalne) - każda linia to IV (tekst lub hex prefixed 0x).")
    args = parser.parse_args()

    ivs = None
    if args.ivs_file:
        ivs = []
        try:
            with open(args.ivs_file, 'r', encoding='utf-8') as fh:
                for line in fh:
                    line = line.strip()
                    if not line:
                        continue
                    if line.startswith("0x") or all(c in "0123456789abcdefABCDEF" for c in line):
                        try:
                            ivs.append(bytes.fromhex(line.replace("0x","")))
                        except Exception:
                            ivs.append(line.encode('utf-8', errors='ignore'))
                    else:
                        ivs.append(line.encode('utf-8', errors='ignore'))
        except Exception as e:
            print("Nie udało się wczytać IVs-file:", e)
            ivs = None

    use_network = (not args.no_network) and _have_psutil
    rng = KeccakBasedCSPRNG(dir_for_files=args.dir, ivs=ivs, timeframe_minutes=args.timeframe,
                            reseed_interval_calls=args.reseed)

    if args.bytes is not None:
        out = rng.get_random_bytes(args.bytes)
        print(out.hex())
    else:
        val = rng.get_random_int(args.bits)
        print(val)


if __name__ == "__main__":
    main()
