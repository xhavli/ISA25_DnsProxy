import subprocess
import tempfile
import time
import os
import signal

TARGET = "./dns"

def start_dns_proxy(filter_content, port=5300):
    # Create temporary filter file
    f = tempfile.NamedTemporaryFile(mode="w", delete=False)
    f.write(filter_content)
    f.close()
    
    # Start DNS proxy
    proc = subprocess.Popen(
        [TARGET, "-s", "dns.google", "-p", str(port), "-f", f.name],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
    )
    
    # Give proxy time to initialize
    time.sleep(0.3)  
    return proc, f.name

def stop_dns_proxy(proc):
    proc.terminate()
    try:
        proc.wait(timeout=1)
    except subprocess.TimeoutExpired:
        proc.kill()

def dig_query(name, port=5300, ipv6=False, query_type="A", query_class=None):
    addr = "::1" if ipv6 else "127.0.0.1"
    cmd = ["dig", f"@{addr}", "-p", str(port), name, query_type, "+short", "+retry=0", "+time=1"]
    
    if query_class:
        cmd.insert(4, f"-c {query_class}")

    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    out, _ = proc.communicate(timeout=2)
    return out.strip()


def test_allowed_domain_ipv4():
    proc, filter_file = start_dns_proxy(filter_content="")
    resp = dig_query("google.com")
    assert resp != "" and resp.replace(".", "").isdigit()  # IP should be returned
    stop_dns_proxy(proc)
    os.unlink(filter_file)

def test_allowed_domain_ipv6():
    proc, filter_file = start_dns_proxy(filter_content="", port=5301)
    resp = dig_query("google.com", port=5301, ipv6=True)
    assert resp != "" and resp.replace(".", "").isdigit()
    stop_dns_proxy(proc)
    os.unlink(filter_file)

def test_blocked_domain():
    proc, filter_file = start_dns_proxy(filter_content="ads.example.com\n")
    resp = dig_query("ads.example.com")
    assert resp == ""  # Should block and reply REFUSED, no IP returned
    stop_dns_proxy(proc)
    os.unlink(filter_file)

def test_qdcount_not_1():
    # This test sends a query with 0 questions: malformed
    proc, filter_file = start_dns_proxy(filter_content="")
    
    # Craft malformed DNS query: header with QDCOUNT=0
    malformed = bytes.fromhex("abcd010000000100000000")
    sock = subprocess.Popen(
        ["socat", "-", f"UDP:127.0.0.1:5300"], stdin=subprocess.PIPE
    )
    sock.stdin.write(malformed)
    sock.stdin.flush()
    time.sleep(0.1)
    sock.terminate()
    
    # The proxy should respond (we could add recv here for deeper validation)
    stop_dns_proxy(proc)
    os.unlink(filter_file)

def test_qtype_not_a():
    proc, filter_file = start_dns_proxy(filter_content="")
    
    # Query AAAA record (IPv6) instead of A
    resp = dig_query("google.com", query_type="AAAA")
    
    # Should be rejected by proxy -> no IP returned
    assert resp == ""
    
    stop_dns_proxy(proc)
    os.unlink(filter_file)

def test_malformed_packet():
    proc, filter_file = start_dns_proxy(filter_content="")
    
    # Send broken DNS packet (only 2 bytes = invalid)
    sock = subprocess.Popen(
        ["socat", "-", f"UDP:127.0.0.1:5300"], stdin=subprocess.PIPE
    )
    sock.stdin.write(b"\x00\x01")  # Too short
    sock.stdin.flush()
    time.sleep(0.1)
    sock.terminate()
    
    stop_dns_proxy(proc)
    os.unlink(filter_file)
