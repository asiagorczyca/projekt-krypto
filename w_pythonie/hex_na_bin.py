hex_file = "output.txt"
bin_file = "output.bin"

with open(hex_file, "r") as f_in, open(bin_file, "wb") as f_out:
    for line in f_in:
        line = line.strip()  # usuwa spacje i \n
        if not line:
            continue
        # Konwertujemy hex -> bytes
        try:
            b = bytes.fromhex(line)
            f_out.write(b)
        except ValueError:
            print(f"Niepoprawna linia: {line}")
