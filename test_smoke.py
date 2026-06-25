#!/usr/bin/env python3
"""Quick smoke test — runs against a server on 127.0.0.1:6380."""
import socket, time, sys

def conn():
    s = socket.socket()
    s.connect(('127.0.0.1', 6380))
    s.settimeout(2)
    return s

def send(s, *args):
    msg = f"*{len(args)}\r\n"
    for a in args:
        msg += f"${len(a)}\r\n{a}\r\n"
    s.sendall(msg.encode())

def recv_line(s):
    data = b""
    while not data.endswith(b"\r\n"):
        data += s.recv(1)
    return data.decode().rstrip("\r\n")

def recv_resp(s):
    line = recv_line(s)
    if line.startswith('+') or line.startswith('-') or line.startswith(':'):
        return line
    if line.startswith('$'):
        n = int(line[1:])
        if n < 0: return "$-1"
        data = s.recv(n + 2).decode().rstrip("\r\n")
        return data
    if line.startswith('*'):
        n = int(line[1:])
        return [recv_resp(s) for _ in range(n)]
    return line

PASS = "\033[32mPASS\033[0m"
FAIL = "\033[31mFAIL\033[0m"

def check(label, got, expected):
    ok = got == expected
    print(f"  [{PASS if ok else FAIL}] {label}: got {got!r}")
    if not ok:
        print(f"         expected {expected!r}")
    return ok

failures = 0

# ---- PING ----
print("PING")
s = conn()
send(s, "PING")
r = recv_resp(s)
failures += not check("PING", r, "+PONG")
s.close()

# ---- SET / GET ----
print("SET/GET")
s = conn()
send(s, "SET", "foo", "bar")
r = recv_resp(s)
failures += not check("SET", r, "+OK")
send(s, "GET", "foo")
r = recv_resp(s)
failures += not check("GET", r, "bar")
s.close()

# ---- SET with EX / GET after expiry ----
print("TTL expiry")
s = conn()
send(s, "SET", "exp", "val", "PX", "300")
recv_resp(s)
send(s, "GET", "exp")
r = recv_resp(s)
failures += not check("GET before expiry", r, "val")
time.sleep(0.4)
send(s, "GET", "exp")
r = recv_resp(s)
failures += not check("GET after expiry", r, "$-1")
s.close()

# ---- DEL ----
print("DEL")
s = conn()
send(s, "SET", "dk", "dv")
recv_resp(s)
send(s, "DEL", "dk")
r = recv_resp(s)
failures += not check("DEL count", r, ":1")
send(s, "GET", "dk")
r = recv_resp(s)
failures += not check("GET after DEL", r, "$-1")
s.close()

# ---- EXISTS ----
print("EXISTS")
s = conn()
send(s, "SET", "ex1", "a")
recv_resp(s)
send(s, "EXISTS", "ex1", "nokey")
r = recv_resp(s)
failures += not check("EXISTS 1+0", r, ":1")
s.close()

# ---- PTTL ----
print("PTTL")
s = conn()
send(s, "SET", "tk", "tv", "PX", "5000")
recv_resp(s)
send(s, "PTTL", "tk")
r = recv_resp(s)
ok = r.startswith(":") and 0 < int(r[1:]) <= 5000
failures += not check("PTTL in range", ok, True)
s.close()

# ---- PUBLISH / SUBSCRIBE ----
print("PubSub")
sub = conn()
pub = conn()
send(sub, "SUBSCRIBE", "chan1")
ack = recv_resp(sub)  # [subscribe, chan1, 1]
failures += not check("SUBSCRIBE ack[0]", ack[0] if isinstance(ack, list) else ack, "subscribe")

send(pub, "PUBLISH", "chan1", "hello")
r = recv_resp(pub)
failures += not check("PUBLISH count", r, ":1")

sub.settimeout(1)
try:
    msg = recv_resp(sub)
    failures += not check("message payload", msg[2] if isinstance(msg, list) else msg, "hello")
except Exception as e:
    print(f"  [{FAIL}] PubSub message: {e}")
    failures += 1
sub.close()
pub.close()

# ---- Summary ----
print()
if failures == 0:
    print(f"\033[32mAll tests passed.\033[0m")
else:
    print(f"\033[31m{failures} test(s) failed.\033[0m")
    sys.exit(1)
