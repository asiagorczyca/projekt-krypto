# Cryptography project 
This project is a complete code to enable communication between two users with the help of a server. Both server and client need the implementation.py file to ensure they work correctly. The aim of this project is to implement a safe way for encrypted end-to-end communication  using Diffie-Hellman key exchange based on large prime numbers and to ensure that these primes are considered safe.

# General information
## Features
- Key exchange - using Diffie-Hellman with GNU Multiple Precision Arithmetic Library for efficiency, sieving to discard bad primes and Miller-Rabin test (probabilistic proof of primality),
- Message encryption - AES-256 in cipher block chaining mode with PKCS#7 padding and an IV that changes for every message, the secret from Diffie-Hellman is hashed using SHA-256 to create a key,
- Message authentication - using HMAC-256 and the method encrypt-then-MAC to reject malicious messages

## How it works
- Server pairs two clients after they input the IP of the server and their chosen usernames
- Then the server starts DH key exchange with Muller-Rabin test for primality (there is a set timeout in case the generation of DH parameters takes too long - the server reverts to using a standard RFC 3526 4096-bit MODP Group to save time) after that both clients compute their public keys to finally compute a shared secret that is then hashed
- Messages are salted, encrypted with AES-256 and signed with HMAC

## Usage

```bash
# Clone the repository
git clone https://github.com/asiagorczyca/projekt-krypto.git

cd projekt-krypto
# install libraries
pip install -r requirements.txt

# to run the server
python server.py

# to run the client (server waits for two users) 
python client.py
```

# Documentation 

## implementation.py 


## server.py 



## client.py 
_This is the client file used for establishing connection with the server, computing the public and private keys, receiving messages and sending them (using encryption and HMAC)_

**Functions**
