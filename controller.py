import socket
import threading

# Nasłuch na broadcast na porcie UDP 3331
udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
udp_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
udp_sock.bind(("", 3331))
print("Listening for broadcast on UDP port 3331...")

# Odbiór wiadomości broadcast
while True:
    data, (ip, port) = udp_sock.recvfrom(1024)
    msg = data.decode('utf-8')
    print(f"Received broadcast from {ip}:{port} - '{msg}'")
    if msg.startswith("HELLO"):
        parts = msg.split()
        if len(parts) >= 2:
            try:
                tcp_port = int(parts[1])
            except ValueError:
                print("Invalid port in HELLO message.")
                continue
            # Odpowiedz „1234” na UDP do nadawcy
            udp_sock.sendto(b"1234", (ip, port))
            print(f"Sent reply '1234' to {ip}:{port}")
            break

# Nawiązanie połączenia TCP do nadawcy broadcastu
tcp_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
print(f"Connecting to {ip}:{tcp_port} via TCP...")
try:
    tcp_sock.connect((ip, tcp_port))
    print("TCP connection established.")
except Exception as e:
    print(f"TCP connection failed: {e}")
    udp_sock.close()
    tcp_sock.close()
    exit(1)

# Wątek odbierający wiadomości TCP
def recv_messages():
    while True:
        try:
            data = tcp_sock.recv(1024)
            if not data:
                print("Connection closed by the peer.")
                break
            print("Peer:", data.decode('utf-8'))
        except Exception as e:
            print(f"Receive error: {e}")
            break

recv_thread = threading.Thread(target=recv_messages, daemon=True)
recv_thread.start()

# Wysyłanie wiadomości wpisanych przez użytkownika
print("You can now chat. Type messages and press Enter.")
try:
    while True:
        line = input()
        if line:
            tcp_sock.send(line.encode('utf-8'))
        if line == "exit":
            break
except KeyboardInterrupt:
    pass

# Sprzątanie zasobów
tcp_sock.close()
udp_sock.close()
print("Chat ended.")
