import socket
import threading

# Client IP and Port configuration
HOST = ''   # Server's IP address (localhost)
PORT = 65432        # Port the server is listening on

def receive_messages(client_socket):
    """Handle incoming messages from the server in a separate thread"""
    while True:
        try:
            data = client_socket.recv(1024)
            if not data:
                print("\nServer closed the connection")
                break
            
            message = data.decode()
            
            # Check if server sent a rejection message
            if "SERVER_FULL" in message:
                print(f"\nServer rejected connection: {message.split(':', 1)[1].strip()}")
                break
            
            print(f"\n{message}\nEnter your message (type 'exit' to quit): ", end="")
            
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
                HOST = input("Podaj adres IP serwera: ")
                client_socket.connect((HOST, PORT))  # Connect to the server
                print(f"Connected to server at {HOST}:{PORT}")
                print("You can now chat with other connected users!")
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
                    
                    client_socket.sendall(message.encode())  # Send the message to the server
                    
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
    print(HOST)
    start_client()