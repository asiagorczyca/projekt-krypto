import socket
import threading
import podstawa_krypto.df_imp.implementation as imp 

PORT = 65432
MAX_CLIENTS = 2

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.connect(("8.8.8.8", 80))
HOST = s.getsockname()[0]
s.close()

CONNECTIONS = []
connections_lock = threading.Lock()
DH_IN_PROGRESS = False

def broadcast_message(sender_addr, message):
    with connections_lock:
        other_client = None
        for client in CONNECTIONS:
            if client['addr'] != sender_addr:
                other_client = client
                break
        
        if other_client:
            client_conn = other_client.get('conn')
            client_addr = other_client.get('addr')
            try:
                client_conn.sendall((message + "\n").encode())
                print(f"Broadcasted to {client_addr}: {message}")
            except (ConnectionResetError, BrokenPipeError):
                print(f"Client {client_addr} disconnected during broadcast")
                CONNECTIONS.remove(other_client)
            
def initiate_diffie_hellman():
    global DH_IN_PROGRESS
    with connections_lock:
        if len(CONNECTIONS) != 2 or DH_IN_PROGRESS:
            return False
        DH_IN_PROGRESS = True

    try:
        print("Both clients connected! Starting Diffie-Hellman key exchange...")
        print("Using RFC 3526 Standard Primes...")
        p, g = imp.get_standard_parameters()

        print(f"Generator g: {g}")

        with connections_lock:
            for client_info in CONNECTIONS:
                try:
                    conn = client_info.get('conn')
                    addr = client_info.get('addr')
                    conn.sendall(f"DH_START:{g}:{p}\n".encode())
                    print(f"Sent DH parameters to {addr}")
                except Exception as e:
                    print(f"Error sending DH params to {addr}: {e}")

        return True
    finally:
        with connections_lock:
            DH_IN_PROGRESS = False

def handle_client(conn, addr):
    print(f"Connection established with {addr}.")
    
    with connections_lock:
        CONNECTIONS.append({'conn': conn, 'addr': addr, 'pub': None})
    
    print(f"Active connections: {len(CONNECTIONS)}")
    
    if len(CONNECTIONS) == 2:
        initiate_diffie_hellman()
    
    try:
        while True:
            data = conn.recv(4096)
            if not data:
                break
                
            message = data.decode().strip()
            
            if message.startswith("PUBLIC_KEY:"):
                public_key = message.split(":")[1]
                print(f"Received public key from {addr}: {public_key[:50]}...")
                
                with connections_lock:
                    for client_info in CONNECTIONS:
                        if client_info['addr'] == addr:
                            client_info['pub'] = public_key 
                            break
                    
                    clients_with_keys = [c for c in CONNECTIONS if c.get('pub') is not None]
                    
                    if len(clients_with_keys) == 2:
                        print("Both clients sent public keys, exchanging keys...")
                        c1, c2 = clients_with_keys[0], clients_with_keys[1]
                        
                        try:
                            c1['conn'].sendall(f"PEER_PUBLIC:{c2['pub']}\n".encode())
                            c2['conn'].sendall(f"PEER_PUBLIC:{c1['pub']}\n".encode())
                            print("Public keys exchanged between clients")
                            
                            threading.Timer(0.5, lambda: broadcast_message(None, "SECURE_CHANNEL_ESTABLISHED: You can now chat securely!")).start()
                        except Exception as e:
                            print(f"Error exchanging public keys: {e}")

            elif not message.startswith(("DH_START:", "PEER_PUBLIC:")):
                broadcast_message(addr, f"User {addr}: {message}")
                
    except (ConnectionResetError, ConnectionAbortedError):
        print(f"Client {addr} has disconnected.")
    except Exception as e:
        print(f"Error with client {addr}: {e}")
    finally:
        with connections_lock:
            CONNECTIONS[:] = [info for info in CONNECTIONS if info['addr'] != addr]
        
        broadcast_message(addr, f"SYSTEM: User {addr} left the chat")
        conn.close()
        print(f"Connection with {addr} closed.")

def start_server():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server_socket:
        server_socket.bind((HOST, PORT))
        server_socket.listen()
        
        print(f"Server listening on {HOST}:{PORT}...")
        
        while True:
            conn, addr = server_socket.accept()
            
            with connections_lock:
                if len(CONNECTIONS) >= MAX_CLIENTS:
                    print(f"Rejecting {addr}: Server full")
                    conn.sendall("SERVER_FULL: Server is at maximum capacity.\n".encode())
                    conn.close()
                    continue
            
            thread = threading.Thread(target=handle_client, args=(conn, addr))
            thread.daemon = True
            thread.start()

if __name__ == "__main__":
    start_server()