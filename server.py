import socket
import threading
import podstawa_krypto.df_imp.implementation as imp 
import podstawa_krypto.df_imp.generowanie_liczb_losowych as gll

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
        
        other_client = None
        for client in CONNECTIONS:
            if client['addr'] != sender_addr:
                other_client = client
                break
        client_conn = other_client.get('conn')
        client_addr = other_client.get('addr')
            
        try:
            client_conn.sendall((message + "\n").encode())
            print(f"Broadcasted to {client_addr}: {message}")
        except (ConnectionResetError, BrokenPipeError):
            print(f"Client {client_addr} disconnected during broadcast")
            CONNECTIONS.remove(client)
            print(f"Removed disconnected client {client_addr}")
            
            
def initiate_diffie_hellman():
    global DH_IN_PROGRESS
    with connections_lock:
        if len(CONNECTIONS) != 2 or DH_IN_PROGRESS:
            return False
        DH_IN_PROGRESS = True

    try:
        print("Both clients connected! Starting Diffie-Hellman key exchange...")
        bits = 4096
        print(f"Generating p ({bits} bits)...")
        p = imp.generate_prime(bits)
        print(f"Generated p: {p}")
        g = imp.find_generator(p)
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
    
    CONNECTIONS.append({'conn': conn, 'addr': addr, 'pub': None})
    print(f"Active connections: {len(CONNECTIONS)}")
    connection_addresses = [client_info['addr'] for client_info in CONNECTIONS]
    print(f"Connected clients: {connection_addresses}")
    
    if len(CONNECTIONS) == 2:
        initiate_diffie_hellman()
    
    try:
        while True:
            data = conn.recv(4096)
            if not data:
                break
                
            message = data.decode()
            print(f"Received from {addr}: {message}")
            
            if message.startswith("PUBLIC_KEY:"):
                public_key = message.split(":")[1]
                print(f"Received public key from {addr}: {public_key}")
                
                for client_info in CONNECTIONS:
                    if client_info['addr'] == addr:
                        client_info['pub'] = public_key 
                        break
                
                notify_secure = False
                client1_conn = client2_conn = None
                client1_pub = client2_pub = None

                clients_with_keys = [client_info for client_info in CONNECTIONS if client_info.get('pub') is not None]
                if len(clients_with_keys) == 2:
                    print("Both clients sent public keys, exchanging keys...")
                    client1_info = clients_with_keys[0]
                    client2_info = clients_with_keys[1]

                    client1_conn = client1_info['conn']
                    client1_pub = client1_info['pub']

                    client2_conn = client2_info['conn']
                    client2_pub = client2_info['pub']

                if client1_conn and client2_conn:
                    try:
                        client1_conn.sendall(f"PEER_PUBLIC:{client2_pub}\n".encode())
                        client2_conn.sendall(f"PEER_PUBLIC:{client1_pub}\n".encode())
                        print("Public keys exchanged between clients")
                        notify_secure = True
                    except Exception as e:
                        print(f"Error exchanging public keys: {e}")

                if notify_secure:
                    broadcast_message(None, "SECURE_CHANNEL_ESTABLISHED: You can now chat securely!")
            
            elif not message.startswith(("DH_START:", "PEER_PUBLIC:")):
                broadcast_message(addr, f"User {addr}: {message}")
                
    except ConnectionResetError:
        print(f"Client {addr} has disconnected.")
    except Exception as e:
        print(f"Error with client {addr}: {e}")
    

    CONNECTIONS[:] = [info for info in CONNECTIONS if info['addr'] != addr]
    print(f"Active connections: {len(CONNECTIONS)}")
    remaining_addresses = [info['addr'] for info in CONNECTIONS]
    print(f"Connected clients: {remaining_addresses}")
    
    broadcast_message(addr, f"SYSTEM: User {addr} left the chat")
    
    conn.close()
    print(f"Connection with {addr} closed.")

def start_server():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server_socket:
        server_socket.bind((HOST, PORT))
        server_socket.listen()
        
        print(f"Server listening on {HOST}:{PORT}...")
        print(f"Waiting for {MAX_CLIENTS} clients to connect for secure chat...")
        
        while True:
            conn, addr = server_socket.accept()
            
            with connections_lock:
                if len(CONNECTIONS) >= MAX_CLIENTS:
                    print(f"Maximum clients ({MAX_CLIENTS}) reached. Rejecting connection from {addr}")
                    conn.sendall("SERVER_FULL: Server is at maximum capacity. Please try again later.\n".encode())
                    conn.close()
                    continue
            
            thread = threading.Thread(target=handle_client, args=(conn, addr))
            thread.daemon = True
            thread.start()

if __name__ == "__main__":
    start_server()