from Cryptodome.Cipher import AES
from Cryptodome.Random import get_random_bytes

with open("tekst_jawny.txt", 'r', encoding='utf-8') as file:
    data_string = file.read()
    data = data_string.encode('utf-8')
key = get_random_bytes(32) #generuje klucz losowy
print(key) #wypisuje klucz wygenerowany losowo
szyfr = AES.new(key, AES.MODE_CTR)
szyfrogram = szyfr.encrypt(data)
with open("szyfrogram.bin", 'wb') as file:
    file.write(szyfr.nonce + szyfrogram)