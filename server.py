import socket
import threading
import podstawa_krypto.df_imp.implementation as imp 
import podstawa_krypto.df_imp.generowanie_liczb_losowych as gll

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.connect(("8.8.8.8", 80))
HOST = s.getsockname()[0]
s.close()

# Server IP and Port configuration
PORT = 65432        # Port for client connections
MAX_CLIENTS = 2

CONNECTIONS = []  # Store client connection dicts: {'conn': socket, 'addr': addr, 'pub': Optional[pub_key]}
connections_lock = threading.Lock()  # Thread lock for safe array access
DH_IN_PROGRESS = False

def broadcast_message(sender_addr, message, exclude_sender=True):
    """Send message to all connected clients except optionally the sender"""
    with connections_lock:
        disconnected_clients = []
        for client_info in CONNECTIONS:
            # client_info is a dict with keys 'conn' and 'addr'
            client_conn = client_info.get('conn')
            client_addr = client_info.get('addr')

            if exclude_sender and sender_addr is not None and client_addr == sender_addr:
                continue
            try:
                client_conn.sendall((message + "\n").encode())
                # debug: confirm successful send
                print(f"Broadcasted to {client_addr}: {message[:60]}")
            except (ConnectionResetError, BrokenPipeError):
                print(f"Client {client_addr} disconnected during broadcast")
                disconnected_clients.append(client_info)
        
        # Remove disconnected clients
        for client in disconnected_clients:
            if client in CONNECTIONS:
                addr = client.get('addr') if isinstance(client, dict) else client
                CONNECTIONS.remove(client)
                print(f"Removed disconnected client {addr}")

def initiate_diffie_hellman():
    """Initiate DH key exchange when both clients are connected"""
    global DH_IN_PROGRESS
    # Quickly check and mark DH start under lock, but do heavy work outside the lock
    with connections_lock:
        if len(CONNECTIONS) != 2 or DH_IN_PROGRESS:
            return False
        DH_IN_PROGRESS = True
        # capture current recipient addresses for later logging; do NOT hold lock while generating primes
        current_clients = [info.get('addr') for info in CONNECTIONS]

    try:
        print("Both clients connected! Starting Diffie-Hellman key exchange...")
        # Generate DH parameters outside the lock (this is CPU-heavy and blocks if done while holding the lock)
        bits = 4096
        print(f"[INFO] Generating p ({bits} bits)...")
        p = imp.generate_prime(bits)
        print(f"Generated p: {p}")
        g = imp.find_generator(p)
        print(f"Generator g: {g}")

        # Send DH parameters to both clients (re-acquire lock briefly)
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
        # ensure flag cleared only if we haven't started an exchange that should persist; clearing here allows retries
        with connections_lock:
            DH_IN_PROGRESS = False

def handle_client(conn, addr):
    print(f"Connection established with {addr}.")
    
    # Add client to connections array as (conn, addr)
    with connections_lock:
        # store as dict for consistent access and optional public key
        CONNECTIONS.append({'conn': conn, 'addr': addr, 'pub': None})
        print(f"Active connections: {len(CONNECTIONS)}")
        connection_addresses = [info['addr'] for info in CONNECTIONS]
        print(f"Connected clients: {connection_addresses}")
    
    # If we now have 2 clients, initiate DH key exchange
    if len(CONNECTIONS) == 2:
        initiate_diffie_hellman()
    
    try:
        while True:
            data = conn.recv(4096)
            if not data:
                break
                
            message = data.decode()
            print(f"Received from {addr}: {message}")
            
            # Handle DH public key from client
            if message.startswith("PUBLIC_KEY:"):
                public_key = message.split(":")[1]
                print(f"Received public key from {addr}: {public_key}")
                
                # Update connection info to include public key
                with connections_lock:
                    for i, client_info in enumerate(CONNECTIONS):
                            if client_info['addr'] == addr:
                                # set public key field
                                CONNECTIONS[i]['pub'] = public_key
                                break
                
                # If both clients have sent their public keys, exchange them
                notify_secure = False
                client1_conn = client2_conn = None
                client1_pub = client2_pub = None
                with connections_lock:
                    # clients that have a public key set
                    clients_with_keys = [info for info in CONNECTIONS if info.get('pub') is not None]
                    if len(clients_with_keys) == 2:
                        print("Both clients sent public keys, exchanging keys...")
                        client1_info = clients_with_keys[0]
                        client2_info = clients_with_keys[1]

                        client1_conn = client1_info['conn']
                        client1_addr = client1_info['addr']
                        client1_pub = client1_info['pub']

                        client2_conn = client2_info['conn']
                        client2_addr = client2_info['addr']
                        client2_pub = client2_info['pub']

                # send each client's public key (do not hold connections_lock while sending or broadcasting)
                if client1_conn and client2_conn:
                    try:
                        client1_conn.sendall(f"PEER_PUBLIC:{client2_pub}\n".encode())
                        client2_conn.sendall(f"PEER_PUBLIC:{client1_pub}\n".encode())
                        print("Public keys exchanged between clients")
                        notify_secure = True
                    except Exception as e:
                        print(f"Error exchanging public keys: {e}")

                # Notify clients that secure channel is ready (outside lock)
                if notify_secure:
                    broadcast_message(None, "SECURE_CHANNEL_ESTABLISHED: You can now chat securely!", exclude_sender=False)
            
            # Handle regular chat messages
            elif not message.startswith(("DH_START:", "PEER_PUBLIC:")):
                # broadcast regular chat message to all other clients
                broadcast_message(addr, f"User {addr}: {message}")
                
    except ConnectionResetError:
        print(f"Client {addr} has disconnected.")
    except Exception as e:
        print(f"Error with client {addr}: {e}")
    
    # Remove client from connections array when connection closes
    with connections_lock:
        # Remove by address
        CONNECTIONS[:] = [info for info in CONNECTIONS if info['addr'] != addr]
        print(f"Active connections: {len(CONNECTIONS)}")
        remaining_addresses = [info['addr'] for info in CONNECTIONS]
        print(f"Connected clients: {remaining_addresses}")
    
    # Notify remaining clients about disconnection
    broadcast_message(addr, f"SYSTEM: User {addr} left the chat", exclude_sender=False)
    
    conn.close()
    print(f"Connection with {addr} closed.")

# Start the server
def start_server():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server_socket:
        server_socket.bind((HOST, PORT))
        server_socket.listen()
        
        print(f"Server listening on {HOST}:{PORT}...")
        print(f"Waiting for {MAX_CLIENTS} clients to connect for secure chat...")
        
        while True:
            conn, addr = server_socket.accept()
            
            # Check if we have room for new clients
            with connections_lock:
                if len(CONNECTIONS) >= MAX_CLIENTS:
                    print(f"Maximum clients ({MAX_CLIENTS}) reached. Rejecting connection from {addr}")
                    conn.sendall("SERVER_FULL: Server is at maximum capacity. Please try again later.\n".encode())
                    conn.close()
                    continue
            
            # Start a new thread for the client
            thread = threading.Thread(target=handle_client, args=(conn, addr))
            thread.daemon = True
            thread.start()

if __name__ == "__main__":
    print(HOST)
    start_server()