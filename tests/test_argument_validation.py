import subprocess
import os

TARGET = "./dns"

def run_dns(args):
    """Run the dns binary with specified args and return stdout and stderr."""
    process = subprocess.Popen(
        [TARGET] + args,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    stdout, stderr = process.communicate()
    return stdout, stderr, process.returncode

def test_missing_server():
    _, stderr, code = run_dns(["-f", "filter.txt"])
    assert "missing required -s" in stderr.lower()
    assert code != 0

def test_missing_filter():
    _, stderr, code = run_dns(["-s", "8.8.8.8"])
    assert "missing required -f" in stderr.lower()
    assert code != 0

def test_invalid_port():
    _, stderr, _ = run_dns(["-s", "8.8.8.8", "-f", "filter.txt", "-p", "abc"])
    assert "contains non-numeric characters" in stderr

def test_port_out_of_range():
    _, stderr, _ = run_dns(["-s", "8.8.8.8", "-f", "filter.txt", "-p", "70000"])
    assert "out of range" in stderr

def test_valid_args():
    # Create a dummy filter file
    with open("filter.txt", "w") as f:
        f.write("")

    stdout, stderr, code = run_dns(["-s", "8.8.8.8", "-f", "filter.txt", "-p", "53"])
    
    assert "ERROR: missing" not in stderr
    assert "ERROR: unknown" not in stderr
    assert "contains non-numeric characters" not in stderr

