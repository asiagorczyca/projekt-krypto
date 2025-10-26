import socket
import threading

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
        for client_conn, client_addr in CONNECTIONS:
            if exclude_sender and client_addr == sender_addr:
                continue
            try:
                client_conn.sendall(message.encode())
            except (ConnectionResetError, BrokenPipeError):
                print(f"Client {client_addr} disconnected during broadcast")
                disconnected_clients.append((client_conn, client_addr))
        
        # Remove disconnected clients
        for client in disconnected_clients:
            if client in CONNECTIONS:
                CONNECTIONS.remove(client)
                print(f"Removed disconnected client {client[1]}")

def handle_client(conn, addr):
    print(f"Connection established with {addr}.")
    
    # Add client to connections array
    with connections_lock:
        CONNECTIONS.append((conn, addr))
        print(f"Active connections: {len(CONNECTIONS)}")
        print([addr for _, addr in CONNECTIONS])
    
    # Notify all clients about new connection
    broadcast_message(addr, f"SYSTEM: User {addr} joined the chat", exclude_sender=False)
    
    while True:
        try:
            data = conn.recv(1024)  # Receive data from the client
            if not data:
                break  # Terminate the connection if no data is received
            
            message = data.decode()
            print(f"Received from {addr}: {message}")
            
            # Broadcast the message to all other clients
            # funkcja do wymiany kluczy 
            broadcast_message(addr, f"User {addr}: {message}")
            
        except ConnectionResetError:
            print(f"Client {addr} has disconnected.")
            break
    
    # Remove client from connections array when connection closes
    with connections_lock:
        CONNECTIONS[:] = [(c, a) for c, a in CONNECTIONS if a != addr]
        print(f"Active connections: {len(CONNECTIONS)}")
        print([addr for _, addr in CONNECTIONS])
    
    # Notify all clients about disconnection
    broadcast_message(addr, f"SYSTEM: User {addr} left the chat", exclude_sender=False)
    
    conn.close()
    print(f"Connection with {addr} closed.")

# Start the server
def start_server():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server_socket:
        server_socket.bind((HOST, PORT))
        server_socket.listen()
        print(f"Server listening on {HOST}:{PORT}...")
        
        while True:
            conn, addr = server_socket.accept()  # Accept a client connection
            
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