import socket
import threading
import time
import multiprocessing
import podstawa_krypto.df_imp.implementation as imp 

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
try:
    s.connect(("8.8.8.8", 80))
    HOST = s.getsockname()[0]
except Exception:
    HOST = "127.0.0.1"
finally:
    s.close()

PORT = 65432
MAX_CLIENTS = 2

CONNECTIONS = []
connections_lock = threading.RLock() 
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
            except Exception:
                if target in CONNECTIONS:
                    CONNECTIONS.remove(target)

def initiate_diffie_hellman():
    global DH_IN_PROGRESS
    with connections_lock:
        if len(CONNECTIONS) != 2 or DH_IN_PROGRESS:
            return False
        DH_IN_PROGRESS = True

    try:
        print("\nStarting Key Exchange...")
        with connections_lock:
            c1 = CONNECTIONS[0]
            c2 = CONNECTIONS[1]
            c2['conn'].sendall(f"PEER_USERNAME:{c1['name']}\n".encode())
            c1['conn'].sendall(f"PEER_USERNAME:{c2['name']}\n".encode())

        bits = 2048 
        p, g = imp.get_parameters_parallel(bits, timeout=10)
        print(f"[DH] Generated parameters p: {p} and g: {g}")
        
        
        with connections_lock:
            for client in CONNECTIONS:
                try:
                    client['conn'].sendall(f"DH_START:{g}:{p}\n".encode())
                except Exception as e:
                    print(f"Error sending params: {e}")
    except Exception as e:
        print(f"DH Initialization Error: {e}")
    finally:
        with connections_lock:
            DH_IN_PROGRESS = False

def handle_client(conn, addr):
    print(f"Connection from {addr}")
    
    with connections_lock:
        CONNECTIONS.append({'conn': conn, 'addr': addr, 'pub': None, 'name': f"User({addr[1]})"})
    
    if len(CONNECTIONS) == 2:
        threading.Thread(target=initiate_diffie_hellman).start()

    buffer = ""
    try:
        while True:
            data = conn.recv(4096)
            if not data: break
            
            buffer += data.decode(errors='ignore')
            while "\n" in buffer:
                message, buffer = buffer.split("\n", 1)
                message = message.strip()
                if not message: continue
                
                if message.startswith("USERNAME:"):
                    new_name = message.split(":", 1)[1]
                    print(f"[{addr}] registered as {new_name}")
                    with connections_lock:
                        for c in CONNECTIONS:
                            if c['addr'] == addr:
                                c['name'] = new_name
                                break
                        broadcast_message(addr, f"PEER_USERNAME:{new_name}")
                    continue

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
                            try:
                                c1['conn'].sendall(f"PEER_PUBLIC:{c2['pub']}\n".encode())
                                c2['conn'].sendall(f"PEER_PUBLIC:{c1['pub']}\n".encode())
                                time.sleep(0.1)
                                broadcast_message(None, "SECURE_CHANNEL_ESTABLISHED")
                            except Exception as e:
                                print(f"Error exchanging keys: {e}")

                elif not message.startswith(("DH_START:", "PEER_PUBLIC:")):
                    broadcast_message(addr, message)

    except Exception as e:
        print(f"Error {addr}: {e}")
    finally:
        with connections_lock:
            CONNECTIONS[:] = [c for c in CONNECTIONS if c['addr'] != addr]
        try:
            broadcast_message(addr, f"User {addr} left.")
        except:
            pass
        conn.close()

def start_server():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        server.bind((HOST, PORT))
    except OSError:
        server.bind(('0.0.0.0', PORT))
        
    server.listen()
    print(f"Server running on {HOST}:{PORT}")
    
    while True:
        conn, addr = server.accept()
        with connections_lock:
            if len(CONNECTIONS) >= MAX_CLIENTS:
                print(f"Rejected {addr} (Server Full)")
                conn.close()
                continue
        threading.Thread(target=handle_client, args=(conn, addr), daemon=True).start()

if __name__ == "__main__":
    multiprocessing.freeze_support()
    start_server()