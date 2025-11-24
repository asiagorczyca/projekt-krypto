from Crypto.Hash import keccak
import os
import time

class KeccakBasedCSPRNG:
    def __init__(self, dir_for_files=None, ivs=None, timeframe_minutes=10, reseed_interval_calls=1024):
        """
        Inicjalizacja Keccak-based CSPRNG
        
        Args:
            dir_for_files: Katalog do zbierania haszy plików
            ivs: Lista Initial Vectors (IV)
            timeframe_minutes: Okres modyfikacji plików w minutach
            reseed_interval_calls: Co ile wywołań reseedować
        """
        self.dir_for_files = dir_for_files
        self.ivs = ivs or []
        self.timeframe_minutes = timeframe_minutes
        self.reseed_interval_calls = reseed_interval_calls
        self.call_count = 0
        self.state = self._init_state()
    
    def _init_state(self):
        """Inicjalizuj stan CSPRNG"""
        h = keccak.new(digest_bits=256)
        h.update(os.urandom(32))
        return h.digest()
    
    def get_random_bytes(self, num_bytes):
        """Wygeneruj losowe bajty"""
        self.call_count += 1
        if self.call_count % self.reseed_interval_calls == 0:
            self.state = self._init_state()
        
        h = keccak.new(digest_bits=256)
        h.update(self.state + os.urandom(16))
        self.state = h.digest()
        
        result = h.digest()
        while len(result) < num_bytes:
            h = keccak.new(digest_bits=256)
            h.update(result)
            result += h.digest()
        
        return result[:num_bytes]
    
    def get_random_int(self, num_bits):
        """Wygeneruj losową liczbę całkowitą"""
        num_bytes = (num_bits + 7) // 8
        random_bytes = self.get_random_bytes(num_bytes)
        return int.from_bytes(random_bytes, 'big') & ((1 << num_bits) - 1)