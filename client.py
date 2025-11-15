import socket
import threading
import podstawa_krypto.df_imp.implementation as imp

# Client IP and Port configuration
HOST = ''   # Server's IP address (localhost)
PORT = 65432        # Port the server is listening on

def receive_messages(client_socket):
    """Handle incoming messages from the server in a separate thread"""
    dh_started = False
    secret = None
    p = None
    g = None
    
    while True:
        try:
            data = client_socket.recv(1024)
            if not data:
                print("\nServer closed the connection")
                break
            
            message = data.decode()

            # The server may send multiple newline-delimited messages in one TCP packet.
            for raw in message.splitlines():
                if not raw:
                    continue

                # Check if server sent a rejection message
                if "SERVER_FULL" in raw:
                    print(f"\nServer rejected connection: {raw.split(':', 1)[1].strip()}")
                    return

                # Handle DH parameters from server (starts when both clients are connected)
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

                # Handle peer's public key
                if raw.startswith("PEER_PUBLIC:"):
                    try:
                        peer_public = int(raw.split(":", 1)[1])
                    except Exception:
                        print(f"\nMalformed PEER_PUBLIC message: {raw}")
                        continue
                    print(f"\nReceived peer's public key: {peer_public}")

                    # Compute shared secret
                    if secret and p:
                        shared_key = imp.compute_shared(secret, peer_public, p)
                        print(f"Computed shared secret: {shared_key}")
                    continue

                # Handle secure channel establishment
                if "SECURE_CHANNEL_ESTABLISHED" in raw:
                    print(f"\n{raw}")
                    print("Enter your secure message (type 'exit' to quit): ", end="")
                    continue

                # Handle regular chat messages and system messages
                print(f"\n{raw}")
                if "joined" in raw or "left" in raw:
                    print("Enter your message (type 'exit' to quit): ", end="")
                else:
                    print("Enter your message (type 'exit' to quit): ", end="")
            
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

# Start the client
def start_client():
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
            
            # Start a thread to receive messages
            receive_thread = threading.Thread(target=receive_messages, args=(client_socket,))
            receive_thread.daemon = True
            receive_thread.start()
            
            while True:
                try:
                    message = input("Enter your message (type 'exit' to quit): ")
                    if message.lower() == 'exit':
                        print("Closing the connection...")
                        break
                    
                    client_socket.sendall(message.encode())
                    
                except KeyboardInterrupt:
                    print("\nClosing the connection...")
                    break
                except Exception as e:
                    print(f"Error sending message: {e}")
                    break
    
    except Exception as e:
        print(f"Unexpected error: {e}")
    
    print("Connection terminated.")

if __name__ == "__main__":
    start_client()