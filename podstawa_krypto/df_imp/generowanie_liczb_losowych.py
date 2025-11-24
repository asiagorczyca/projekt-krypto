import argparse
import pathlib
import sys

if __package__:
    from .keccak_based_csprng import KeccakBasedCSPRNG
else:
    # umożliwia uruchomienie: python podstawa_krypto/df_imp/generowanie_liczb_losowych.py
    current_dir = pathlib.Path(__file__).resolve().parent
    if str(current_dir) not in sys.path:
        sys.path.insert(0, str(current_dir))
    from keccak_based_csprng import KeccakBasedCSPRNG

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
