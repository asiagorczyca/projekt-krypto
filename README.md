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
# Diffie-Hellman Parameter Generator

This Python module provides a robust and high-performance toolkit for the Diffie-Hellman key exchange protocol. It handles the generation of secure parameters (Safe Primes) and the calculation of shared secrets between two parties.

## Overview

The script is designed to generate custom, cryptographically secure parameters ($p$ and $g$) using parallel processing for speed. To ensure reliability, it features an **automatic fallback mechanism**: if custom parameters cannot be generated within a specified timeout, the system instantly switches to industry-standard parameters defined in **RFC 3526 (4096-bit)**.

## Key Features

*   **High Performance:** Utilizes the `gmpy2` library for extremely fast arithmetic operations.
*   **Parallel Generation:** Leverages multiple CPU cores to search for Safe Primes efficiently.
*   **Reliable Fallback:** Guarantees valid parameters are always returned, even if the generation times out.
*   **Complete Toolkit:** Includes helper functions to generate private keys, compute public keys, and derive the final shared secret.

## Instalation

This module requires the `gmpy2` library.

```bash
pip install gmpy2
```

## Usage
```python
import dh_module  # Assuming you saved the file as dh_module.py

# 1. Get Parameters (p and g)
# Tries to generate custom params for 10 seconds, then falls back to standard if needed.
print("Negotiating parameters...")
p, g = dh_module.get_parameters_parallel(bits=2048, timeout=10)

# 2. Alice generates her keys
alice_priv = dh_module.generate_private(p)
alice_pub = dh_module.compute_public(g, alice_priv, p)

# 3. Bob generates his keys
bob_priv = dh_module.generate_private(p)
bob_pub = dh_module.compute_public(g, bob_priv, p)

# 4. Exchange and Compute Shared Secret
alice_secret = dh_module.compute_shared(alice_priv, bob_pub, p)
bob_secret = dh_module.compute_shared(bob_priv, alice_pub, p)

# Verify
if alice_secret == bob_secret:
    print("Success! Shared secret established.")
else:
    print("Error: Secrets do not match.")
```
## server.py
# Secure Chat Server (DH Orchestrator)

This script acts as the central hub for a secure, two-party chat application. It manages network connections and orchestrates the Diffie-Hellman key exchange process, allowing two clients (Alice and Bob) to establish a secure communication channel.

## Overview

The server listens for incoming connections and waits until exactly two clients join. Once both parties are connected, the server utilizes the **Diffie-Hellman Implementation** module to generate cryptographically secure parameters ($p$ and $g$) and distributes them to both clients. It then acts as a relay for public keys and encrypted messages.

## Key Features

*   **Automatic Network Discovery:** Automatically detects the local IP address (LAN) to host the server, falling back to `127.0.0.1` if offline.
*   **Centralized Parameter Generation:** Offloads the heavy computational work of generating Safe Primes ($p$) and generators ($g$) from the clients to the server.
*   **Connection Management:** Strictly limits the session to 2 clients to ensure a one-on-one secure channel.
*   **Protocol Handling:** Manages the handshake workflow:
    1.  Wait for 2 clients.
    2.  Generate and broadcast $p, g$.
    3.  Exchange Public Keys between clients.
    4.  Relay chat messages.

## Requirements

This script depends on the Diffie-Hellman module created previously.

1.  Ensure you have the folder structure matching the import in the code:
    `podstawa_krypto/df_imp/implementation.py`
2.  Alternatively, adjust the import line inside the script (`import podstawa_krypto.df_imp.implementation as imp`) to match your current project structure.

## Usage

Run the server script using Python. It will display the IP address and Port you should share with the clients.

```bash
python server.py
Server running on 192.168.1.15:65432
Connection from ('192.168.1.20', 50321)
Connection from ('192.168.1.25', 51002)

Starting Key Exchange...
[DH] Generated parameters p: ... and g: ...
Exchanging Public Keys...
```

## client.py 
_This is the client file used for establishing connection with the server, computing the public and private keys, receiving messages and sending them (using encryption and HMAC)_

**Functions**
