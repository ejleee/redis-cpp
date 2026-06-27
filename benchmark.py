#!/usr/bin/env python3
"""
Benchmark script for redis-clone.
Tests latency (GET/SET) and concurrent client throughput.
Run with the server already started: ./build/redis_clone
"""
import socket, time, threading, statistics, sys

HOST, PORT = "127.0.0.1", 6380

def make_cmd(*args):
    msg = f"*{len(args)}\r\n"
    for a in args:
        msg += f"${len(a)}\r\n{a}\r\n"
    return msg.encode()

def send_recv(sock, *args):
    sock.sendall(make_cmd(*args))
    return sock.recv(256)

def new_conn():
    s = socket.socket()
    s.connect((HOST, PORT))
    s.settimeout(5)
    return s

# ─── Latency benchmark ────────────────────────────────────────────────────────

def bench_latency(n=1000):
    print(f"\n── Latency ({n} ops each) ──")
    s = new_conn()

    # SET
    times = []
    for i in range(n):
        t0 = time.perf_counter()
        send_recv(s, "SET", f"key:{i}", "value")
        times.append((time.perf_counter() - t0) * 1000)
    print(f"  SET  avg={statistics.mean(times):.3f}ms  "
          f"p50={statistics.median(times):.3f}ms  "
          f"p99={sorted(times)[int(n*0.99)]:.3f}ms")

    # GET
    times = []
    for i in range(n):
        t0 = time.perf_counter()
        send_recv(s, "GET", f"key:{i}")
        times.append((time.perf_counter() - t0) * 1000)
    print(f"  GET  avg={statistics.mean(times):.3f}ms  "
          f"p50={statistics.median(times):.3f}ms  "
          f"p99={sorted(times)[int(n*0.99)]:.3f}ms")

    s.close()

# ─── Concurrent clients benchmark ────────────────────────────────────────────

def client_worker(client_id, n_ops, results, errors):
    try:
        s = new_conn()
        t0 = time.perf_counter()
        for i in range(n_ops):
            send_recv(s, "SET", f"c{client_id}:k{i}", "v")
            send_recv(s, "GET", f"c{client_id}:k{i}")
        elapsed = time.perf_counter() - t0
        results.append(n_ops * 2 / elapsed)  # ops/sec
        s.close()
    except Exception as e:
        errors.append(str(e))

def bench_concurrency(n_clients=500, ops_per_client=20):
    print(f"\n── Concurrency ({n_clients} simultaneous clients, {ops_per_client} ops each) ──")
    results, errors = [], []
    threads = [
        threading.Thread(target=client_worker, args=(i, ops_per_client, results, errors))
        for i in range(n_clients)
    ]
    t0 = time.perf_counter()
    for t in threads: t.start()
    for t in threads: t.join()
    elapsed = time.perf_counter() - t0

    if errors:
        print(f"  ERRORS ({len(errors)}): {errors[:3]}")
    total_ops = n_clients * ops_per_client * 2
    print(f"  Clients:      {n_clients}")
    print(f"  Total ops:    {total_ops:,}")
    print(f"  Elapsed:      {elapsed:.2f}s")
    print(f"  Throughput:   {total_ops/elapsed:,.0f} ops/sec")
    print(f"  Errors:       {len(errors)}")
    print(f"  Success rate: {(n_clients - len(errors)) / n_clients * 100:.1f}%")

# ─── TTL correctness check ────────────────────────────────────────────────────

def check_ttl():
    print(f"\n── TTL correctness ──")
    s = new_conn()
    send_recv(s, "SET", "ttlkey", "val", "PX", "200")
    r = send_recv(s, "GET", "ttlkey").decode()
    before_ok = "val" in r
    time.sleep(0.25)
    r = send_recv(s, "GET", "ttlkey").decode()
    after_ok = "$-1" in r
    s.close()
    print(f"  GET before expiry: {'OK' if before_ok else 'FAIL'}")
    print(f"  GET after expiry:  {'OK (nil)' if after_ok else 'FAIL'}")
    return before_ok and after_ok

if __name__ == "__main__":
    print(f"Connecting to {HOST}:{PORT}...")
    try:
        s = new_conn(); s.close()
    except Exception:
        print("ERROR: could not connect. Is the server running?"); sys.exit(1)

    bench_latency()
    check_ttl()
    bench_concurrency()
    print()
