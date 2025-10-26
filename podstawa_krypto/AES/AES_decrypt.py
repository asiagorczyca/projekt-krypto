from Cryptodome.Cipher import AES
from Cryptodome.Random import get_random_bytes

with open("szyfrogram.bin", 'rb') as file:
    nonce = file.read(8)
    data = file.read()
#klucz jest kopiowany z printa z pierwszej funkcji
key = b'\xfc\xe2\xf8\x84\xdd\r\xf5\x87,1\x1d\xf4a\xca1\x17%\xea1\xfd)*ZJq\x01m\xa8\xe9B\x00\x1a'
szyfr = AES.new(key, AES.MODE_CTR, nonce=nonce)
tekst_jawny_bity = szyfr.decrypt(data)
tekst_jawny = tekst_jawny_bity.decode('utf-8')
with open("wynik.txt", 'w', encoding='utf-8') as file:
    file.write(tekst_jawny)