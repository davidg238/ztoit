#!/usr/bin/env python3
# tcp_echo6.py: TCP echo for ztoit's Thread TCP tests (ztoit#4), the TCP twin of
# rete's udp_echo6.py. Listens on [::]:7777 (dual-stack) and reflects every byte
# verbatim on each connection until the peer closes. One thread per connection.
# Copy of record: ztoit tools/gateway/; deployed to gw85224-01 ~/.local/bin/ under
# the user unit tcp-echo6.service (tools/gateway/README.md).
import socket
import sys
import threading

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 7777

def serve(conn, addr):
    total = 0
    with conn:
        while True:
            data = conn.recv(4096)
            if not data:
                break
            conn.sendall(data)
            total += len(data)
    print(f"echo {total}B <-> [{addr[0]}]:{addr[1]}", flush=True)

s = socket.socket(socket.AF_INET6, socket.SOCK_STREAM)
s.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("::", PORT))
s.listen(8)
print(f"tcp_echo6 listening on [::]:{PORT}", flush=True)
while True:
    conn, addr = s.accept()
    threading.Thread(target=serve, args=(conn, addr), daemon=True).start()
