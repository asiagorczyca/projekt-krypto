import socket
import threading
import podstawa_krypto.df_imp.implementation as imp 

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.connect(("8.8.8.8", 80))
HOST = s.getsockname()[0]
s.close()

# Server IP and Port configuration
PORT = 65432        # Port for client connections
MAX_CLIENTS = 2

CONNECTIONS = []  # Store client connections and addresses
connections_lock = threading.Lock()  # Thread lock for safe array access

def broadcast_message(sender_addr, message, exclude_sender=True):
    """Send message to all connected clients except optionally the sender"""
    with connections_lock:
        disconnected_clients = []
        for client_info in CONNECTIONS:
            client_conn = client_info[0]
            client_addr = client_info[1]
            
            if exclude_sender and client_addr == sender_addr:
                continue
            try:
                client_conn.sendall(message.encode())
            except (ConnectionResetError, BrokenPipeError):
                print(f"Client {client_addr} disconnected during broadcast")
                disconnected_clients.append(client_info)
        
        # Remove disconnected clients
        for client in disconnected_clients:
            if client in CONNECTIONS:
                CONNECTIONS.remove(client)
                print(f"Removed disconnected client {client[1]}")

def initiate_diffie_hellman():
    """Initiate DH key exchange when both clients are connected"""
    with connections_lock:
        if len(CONNECTIONS) == 2:
            print("Both clients connected! Starting Diffie-Hellman key exchange...")
            
            # Generate DH parameters
            bits = 2088
            print(f"[INFO] Generating p ({bits} bits)...")
            p = imp.generate_prime(bits)
            print(f"Generated p: {p}")
            g = imp.find_generator(p)
            print(f"Generator g: {g}")
            
            # Send DH parameters to both clients
            for client_info in CONNECTIONS:
                try:
                    conn = client_info[0]
                    addr = client_info[1]
                    conn.sendall(f"DH_START:{g}:{p}".encode())
                    print(f"Sent DH parameters to {addr}")
                except Exception as e:
                    print(f"Error sending DH params to {addr}: {e}")
            
            return True
    return False

def handle_client(conn, addr):
    print(f"Connection established with {addr}.")
    
    # Add client to connections array as (conn, addr)
    with connections_lock:
        CONNECTIONS.append((conn, addr))
        print(f"Active connections: {len(CONNECTIONS)}")
        connection_addresses = [info[1] for info in CONNECTIONS]
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
                        if client_info[1] == addr:
                            # Update to (conn, addr, public_key)
                            CONNECTIONS[i] = (conn, addr, public_key)
                            break
                
                # If both clients have sent their public keys, exchange them
                with connections_lock:
                    clients_with_keys = [info for info in CONNECTIONS if len(info) == 3]
                    if len(clients_with_keys) == 2:
                        print("Both clients sent public keys, exchanging keys...")
                        client1_info = clients_with_keys[0]
                        client2_info = clients_with_keys[1]
                        
                        client1_conn, client1_addr, client1_pub = client1_info
                        client2_conn, client2_addr, client2_pub = client2_info
                        
                        # Send each client the other's public key
                        try:
                            client1_conn.sendall(f"PEER_PUBLIC:{client2_pub}".encode())
                            client2_conn.sendall(f"PEER_PUBLIC:{client1_pub}".encode())
                            print("Public keys exchanged between clients")
                            
                            # Notify clients that secure channel is ready
                            broadcast_message(None, "SECURE_CHANNEL_ESTABLISHED: You can now chat securely!", exclude_sender=False)
                        except Exception as e:
                            print(f"Error exchanging public keys: {e}")
            
            # Handle regular chat messages
            elif not message.startswith(("DH_START:", "PEER_PUBLIC:")):
                broadcast_message(addr, f"User {addr}: {message}")
                
    except ConnectionResetError:
        print(f"Client {addr} has disconnected.")
    except Exception as e:
        print(f"Error with client {addr}: {e}")
    
    # Remove client from connections array when connection closes
    with connections_lock:
        # Remove by address regardless of whether it's 2-tuple or 3-tuple
        CONNECTIONS[:] = [info for info in CONNECTIONS if info[1] != addr]
        print(f"Active connections: {len(CONNECTIONS)}")
        remaining_addresses = [info[1] for info in CONNECTIONS]
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
                    conn.sendall("SERVER_FULL: Server is at maximum capacity. Please try again later.".encode())
                    conn.close()
                    continue
            
            # Start a new thread for the client
            thread = threading.Thread(target=handle_client, args=(conn, addr))
            thread.daemon = True
            thread.start()

if __name__ == "__main__":
    print(HOST)
    start_server()