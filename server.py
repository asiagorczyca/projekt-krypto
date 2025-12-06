import socket
import threading
import time
import multiprocessing
import podstawa_krypto.df_imp.implementation as imp 

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.connect(("8.8.8.8", 80))
HOST = s.getsockname()[0]
s.close()

PORT = 65432
MAX_CLIENTS = 2

CONNECTIONS = []
connections_lock = threading.Lock()
DH_IN_PROGRESS = False

def broadcast_message(sender_addr, message):
    with connections_lock:
        target = None
        for client in CONNECTIONS:
            if client['addr'] != sender_addr:
                target = client
                break
        
        if target:
            try:
                target['conn'].sendall((message + "\n").encode())
                print(f"Broadcasted to {target['addr']}: {message}")
            except Exception:
                CONNECTIONS.remove(target)

def initiate_diffie_hellman():
    global DH_IN_PROGRESS
    with connections_lock:
        if len(CONNECTIONS) != 2 or DH_IN_PROGRESS:
            return False
        DH_IN_PROGRESS = True

    try:
        print("\nStarting Key Exchange")
        
        bits = 2048 
        print(f"Generating {bits}")
        
        p, g = imp.get_parameters_parallel(bits)
        
        print(f"Final p: {p}")
        print(f"Final g: {g}")

        with connections_lock:
            for client in CONNECTIONS:
                try:
                    conn = client.get('conn')
                    conn.sendall(f"DH_START:{g}:{p}\n".encode())
                except Exception as e:
                    print(f"Error sending params: {e}")

        return True
    finally:
        with connections_lock:
            DH_IN_PROGRESS = False

def handle_client(conn, addr):
    print(f"Connection from {addr}")
    with connections_lock:
        CONNECTIONS.append({'conn': conn, 'addr': addr, 'pub': None})
    
    if len(CONNECTIONS) == 2:
        threading.Thread(target=initiate_diffie_hellman).start()

    try:
        while True:
            data = conn.recv(4096)
            if not data: break
            message = data.decode().strip()
            print(f"[{addr}] {message}")

            if message.startswith("PUBLIC_KEY:"):
                key = message.split(":")[1]
                with connections_lock:
                    for c in CONNECTIONS:
                        if c['addr'] == addr:
                            c['pub'] = key
                            break
                    
                    ready = [c for c in CONNECTIONS if c['pub']]
                    if len(ready) == 2:
                        print("Exchanging Public Keys...")
                        c1, c2 = ready[0], ready[1]
                        c1['conn'].sendall(f"PEER_PUBLIC:{c2['pub']}\n".encode())
                        c2['conn'].sendall(f"PEER_PUBLIC:{c1['pub']}\n".encode())
                        time.sleep(0.1)
                        broadcast_message(None, "SECURE_CHANNEL_ESTABLISHED")

            elif not message.startswith(("DH_START:", "PEER_PUBLIC:")):
                broadcast_message(addr, f"User {addr}: {message}")

    except Exception as e:
        print(f"Error {addr}: {e}")
    finally:
        with connections_lock:
            CONNECTIONS[:] = [c for c in CONNECTIONS if c['addr'] != addr]
        broadcast_message(addr, f"User {addr} left.")
        conn.close()

def start_server():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((HOST, PORT))
    server.listen()
    print(f"Server running on {HOST}:{PORT}")
    
    while True:
        conn, addr = server.accept()
        with connections_lock:
            if len(CONNECTIONS) >= MAX_CLIENTS:
                conn.close()
                continue
        threading.Thread(target=handle_client, args=(conn, addr), daemon=True).start()

if __name__ == "__main__":
    multiprocessing.freeze_support()
    start_server()