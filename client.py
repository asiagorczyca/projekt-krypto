# client.py (poprawiona wersja)
import socket
import threading
import podstawa_krypto.df_imp.implementation as imp
import hashlib
from Crypto.Cipher import AES
from Crypto.Util.Padding import pad, unpad
from base64 import b64encode, b64decode
from random import randint,shuffle
HOST = ''   # zostawiam, ale poniżej nadpisuję w start_client (tak jak w Twoim oryginale)
PORT = 65432

# Globalna zmienna przechowująca hash(shared_secret) jako bajty; ustawiana po wymianie DH
hash_z_shared_key = None

# Stały IV (zgodnie z Twoim założeniem 'obie strony mają stałe iv')
# UWAGA: w praktyce: nie używaj stałego IV dla CBC. Tu robimy to świadomie (Twoje założenie).
IV = b'0123456789abcdef'  # 16 bajtów

def decrypt_message_if_enc(raw_text: str):
    """Jeżeli raw_text zawiera tag 'ENC:' spróbuj odszyfrować treść; zwróć krotkę (ok, text_or_error, decrypted_bool)."""
    global hash_z_shared_key
    if "ENC:" not in raw_text:
        return raw_text, False

    try:
        # znajdź ostatnie wystąpienie 'ENC:' i pobierz to, co po nim (bez dodatkowych spacji)
        idx = raw_text.rfind("ENC:")
        b64part = raw_text[idx + len("ENC:"):].strip()
        ct = b64decode(b64part)
    except Exception as e:
        return f"[ENC decode error: {e}] {raw_text}", False

    if not hash_z_shared_key:
        return "[ENCRYPTED MESSAGE: brak wspólnego klucza — nie można odszyfrować] " + raw_text, False

    try:
        key = hash_z_shared_key[:16]  # AES-128
        cipher = AES.new(key, AES.MODE_CBC, IV)
        pt_padded = cipher.decrypt(ct)
        pt = unpad(pt_padded, AES.block_size).decode('utf-8', errors='replace')
        return pt, True
    except Exception as e:
        return f"[Decrypt error: {e}] {raw_text}", False

def receive_messages(client_socket):
    """Handle incoming messages from the server in a separate thread"""
    dh_started = False
    secret = None
    p = None
    g = None
    global hash_z_shared_key

    while True:
        try:
            data = client_socket.recv(4096)
            if not data:
                print("\nServer closed the connection")
                break

            message = data.decode(errors='replace')

            # Server może wysłać kilka linii naraz
            for raw in message.splitlines():
                if not raw:
                    continue

                # rejection
                if raw.startswith("SERVER_FULL"):
                    print(f"\nServer rejected connection: {raw.split(':', 1)[1].strip()}")
                    return

                # DH_START
                if raw.startswith("DH_START:") and not dh_started:
                    parts = raw.split(":", 2)
                    try:
                        g = int(parts[1])
                        p = int(parts[2])
                    except Exception:
                        print(f"\nMalformed DH_START message: {raw}")
                        continue
                    print(f"\n=== DIFFIE-HELLMAN KEY EXCHANGE STARTED ===")
                    print(f"Received DH parameters - g: {g}, p: {p}")

                    # Generate private key and compute public key
                    secret = imp.generate_private(p)
                    public_key = imp.compute_public(g, secret, p)

                    # Send public key to server
                    client_socket.sendall(f"PUBLIC_KEY:{public_key}\n".encode())
                    print(f"Generated private key and sent public key: {public_key}")
                    dh_started = True
                    continue

                # PEER_PUBLIC
                if raw.startswith("PEER_PUBLIC:"):
                    try:
                        peer_public = int(raw.split(":", 1)[1])
                    except Exception:
                        print(f"\nMalformed PEER_PUBLIC message: {raw}")
                        continue
                    print(f"\nReceived peer's public key: {peer_public}")

                    # Compute shared secret
                    if secret and p:
                        shared_int = imp.compute_shared(secret, peer_public, p)
                        # Convert integer -> bytes deterministically
                        if shared_int == 0:
                            shared_bytes = b'\x00'
                        else:
                            blen = (shared_int.bit_length() + 7) // 8
                            shared_bytes = shared_int.to_bytes(blen, 'big')
                        # Hash the shared secret to derive symmetric key material
                        hash_z_shared_key = hashlib.sha3_256(shared_bytes).digest()
                        print(f"Computed shared secret and derived key (sha3_256).")
                        IV = hashlib.sha3_256((hash_z_shared_key)).digest()[:16]
                        # For debugging you previously printed raw digest; keep short info only
                        print(f"[INFO] Derived key (SHA3-256) length: {len(hash_z_shared_key)} bytes.")
                        print(f"[INFO] Derived key (SHA3-256) : {hash_z_shared_key} bytes.")
                    else:
                        print("Brak prywatnego klucza lokalnego lub p; nie można compute_shared.")
                    continue

                # SECURE_CHANNEL_ESTABLISHED
                if "SECURE_CHANNEL_ESTABLISHED" in raw:
                    print(f"\n{raw}")
                    print("Enter your secure message (type 'exit' to quit): ", end="", flush=True)
                    continue

                # Normal / System messages or broadcasted messages
                # Try to detect and decrypt ENC: payloads
                decrypted_text, was_decrypted = decrypt_message_if_enc(raw)
                if was_decrypted:
                    indeks=decrypted_text.find("!@#$%^&*()!@#$%^&*()")
                    decrypted_text=decrypted_text[indeks+len("!@#$%^&*()!@#$%^&*()"):]
                    print(f"\n[DECRYPTED] {decrypted_text}")
                else:
                    print(f"\n{decrypted_text}")

                print("Enter your message (type 'exit' to quit): ", end="", flush=True)

        except ConnectionResetError:
            print("\nConnection reset by server")
            break
        except ConnectionAbortedError:
            print("\nConnection aborted")
            break
        except BrokenPipeError:
            print("\nConnection broken")
            break
        except Exception as e:
            print(f"\nCommunication error: {e}")
            break

def start_client():
    global hash_z_shared_key,IV
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as client_socket:
            try:
                HOST = input("Input server's IP address: ")
                client_socket.connect((HOST, PORT))
                print(f"Connected to server at {HOST}:{PORT}")
                print("Waiting for another user to connect...")
                print("Type 'exit' to quit\n")

            except ConnectionRefusedError:
                print("Connection refused: Server is not running or not accepting connections")
                return
            except Exception as e:
                print(f"Connection error: {e}")
                return

            # Start receiver thread
            receive_thread = threading.Thread(target=receive_messages, args=(client_socket,))
            receive_thread.daemon = True
            receive_thread.start()

            # Main send loop
            while True:
                try:
                    message = input("Enter your message (type 'exit' to quit): ")
                    if message.lower() == 'exit':
                        print("Closing the connection...")
                        break
                    elif message != '':
                        message=str(randint(10**100,10**101-1))+"!@#$%^&*()!@#$%^&*()"+message
                        # Jeśli nie ma jeszcze wygenerowanego wspólnego klucza, ostrzeż użytkownika i wyślij jawnie
                        if not hash_z_shared_key:
                            # Możesz zdecydować czy wysyłać jawny tekst czy blokować — tu wyślę jawny (niezaszyfrowany) z informacją
                            print("[UWAGA] Nie ma jeszcze wspólnego klucza. Wiadomość zostanie wysłana jawnie.")
                            client_socket.sendall((message + "\n").encode())
                            continue

                        # Szyfruj i wyślij: ENC:<base64(ciphertext)>\n
                        key = hash_z_shared_key[:16]  # AES-128
                        cipher = AES.new(key, AES.MODE_CBC, IV)
                        IV = hashlib.sha3_256((hash_z_shared_key)).digest()[:16]
                        ciphertext = cipher.encrypt(pad(message.encode('utf-8'), AES.block_size))
                        payload = "ENC:" + b64encode(ciphertext).decode()
                        client_socket.sendall((payload + "\n").encode())
                    else:
                        continue

                except KeyboardInterrupt:
                    print("\nClosing the connection...")
                    break
                except Exception as e:
                    print(f"Error sending message: {e}")
                    break

    except Exception as e:
        print(f"Unexpected error: {e}")

    print("Connection terminated.")

start_client()
