import socket
import threading
import podstawa_krypto.df_imp.implementation as imp
import hashlib
from Crypto.Cipher import AES
from Crypto.Util.Padding import pad, unpad
from Crypto.Random import get_random_bytes
from base64 import b64encode, b64decode
from random import randint
import os
import hmac 

HOST = ''
PORT = 65432

hash_z_shared_key = None
peer_name = "Peer"

def decrypt_message_if_enc(raw_text: str):
    global hash_z_shared_key
    if "ENC:" not in raw_text:
        return raw_text, False
    
    try:
        content = raw_text.split("ENC:", 1)[1].strip()
        
        if ":" not in content:
            return "[Error} Malformed message format", False
            
        received_signature, b64_part = content.split(":", 1)

        if not hash_z_shared_key:
            return "[Error} No shared key established yet", False


        computed_signature = hmac.new(
            hash_z_shared_key, 
            b64_part.encode('utf-8'), 
            hashlib.sha256
        ).hexdigest()

        if not hmac.compare_digest(computed_signature, received_signature):
            return "[Error] HMAC Signature Invalid! Message tampered", False
        
        encrypted_data = b64decode(b64_part)
        
        if len(encrypted_data) < 16:
            return "[Error] Data too short", False
            
        iv = encrypted_data[:16]
        ciphertext = encrypted_data[16:]

        key = hash_z_shared_key
        cipher = AES.new(key, AES.MODE_CBC, iv)
        pt_padded = cipher.decrypt(ciphertext)
        pt = unpad(pt_padded, AES.block_size).decode('utf-8')
        return pt, True

    except Exception as e:
        return f"[Error] Decryption Failed: {e}", False

def receive_messages(client_socket):
    global hash_z_shared_key, peer_name
    dh_secret = None
    p = None
    
    buffer = ""

    while True:
        try:
            data = client_socket.recv(4096)
            if not data: break
            
            buffer += data.decode(errors='ignore')
            
            while "\n" in buffer:
                message, buffer = buffer.split("\n", 1)
                message = message.strip()
                if not message: continue

                if message.startswith("PEER_USERNAME:"):
                    new_peer = message.split(":", 1)[1]
                    peer_name = new_peer
                    print(f"\n[System] You are chatting with {peer_name}")
                    print("You: ", end="", flush=True)
                    continue

                if message.startswith("DH_START:"):
                    parts = message.split(":")
                    g = int(parts[1])
                    p = int(parts[2])
                    
                    print(f"\n[System] Key Exchange started...")
                    dh_secret = imp.generate_private(p)
                    public_key = imp.compute_public(g, dh_secret, p)
                    
                    client_socket.sendall(f"PUBLIC_KEY:{public_key}\n".encode())
                    continue

                if message.startswith("PEER_PUBLIC:"):
                    peer_public = int(message.split(":")[1])
                    if dh_secret and p:
                        shared_int = imp.compute_shared(dh_secret, peer_public, p)
                        shared_bytes = shared_int.to_bytes((shared_int.bit_length() + 7) // 8, 'big')
                        hash_z_shared_key = hashlib.sha256(shared_bytes).digest()
                        print(f"[System] Secure Connection Established.")
                    continue


                decrypted_text, was_decrypted = decrypt_message_if_enc(message)
                
                if was_decrypted and "!@#$%^&*()" in decrypted_text:
                    parts = decrypted_text.split("!@#$%^&*()!@#$%^&*()")
                    if len(parts) > 1:
                        decrypted_text = parts[-1]

                if was_decrypted:
                    print(f"\r[{peer_name}]: {decrypted_text}")
                elif not message.startswith("User"):
                    pass

                print("You: ", end="", flush=True)

        except Exception as e:
            print(f"\nError: {e}")
            break
    os._exit(0)

def start_client():
    global hash_z_shared_key
    
    host_ip = input("Enter Server IP: ").strip()
    my_username = input("Choose a username: ").strip()
    
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        client_socket.connect((host_ip, PORT))
    except Exception as e:
        print(f"[System] Could not connect: {e}")
        return

    client_socket.sendall(f"USERNAME:{my_username}\n".encode())

    print(f"[System] Connected to {host_ip}:{PORT} as {my_username}")
    print("[System] Waiting for partner...")
    
    threading.Thread(target=receive_messages, args=(client_socket,), daemon=True).start()

    while True:
        try:
            msg = input("You: ")            
            if msg.lower() == 'exit':
                break
            
            if not msg: continue

            if hash_z_shared_key:
                iv = get_random_bytes(16)
                cipher = AES.new(hash_z_shared_key, AES.MODE_CBC, iv)
                
                salt = str(randint(10**10, 10**11)) + "!@#$%^&*()!@#$%^&*()"
                full_msg = salt + msg
                
                ciphertext = cipher.encrypt(pad(full_msg.encode('utf-8'), AES.block_size))
                
                payload_str = b64encode(iv + ciphertext).decode('utf-8')

                signature = hmac.new(
                    hash_z_shared_key, 
                    payload_str.encode('utf-8'), 
                    hashlib.sha256
                ).hexdigest()
                
                final_packet = f"ENC:{signature}:{payload_str}\n"
                client_socket.sendall(final_packet.encode())
            else:
                print("[System] Waiting for secure connection...")

        except KeyboardInterrupt:
            break
        except Exception as e:
            print(f"Send Error: {e}")
            break

    client_socket.close()

if __name__ == "__main__":
    start_client()