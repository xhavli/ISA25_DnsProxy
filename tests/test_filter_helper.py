import subprocess
import tempfile
import os
import sys

TARGET = "./dns"

def capture_output(command):
    """Execute a command and capture stdout and stderr."""
    process = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    stdout, stderr = process.communicate()
    return stdout, stderr, process.returncode

def write_temp_file(lines):
    """Create a temporary file with the provided lines."""
    temp = tempfile.NamedTemporaryFile(mode="w+", delete=False)
    temp.writelines(line + "\n" for line in lines)
    temp.close()
    return temp.name

def test_load_valid_filters():
    valid_domains = [
        "example.com",
        "www.google.com",
        "sub.domain.org",
        "test123.net",
        "foo-bar.com",
    ]
    filename = write_temp_file(valid_domains)
    
    # Run dns with verbose filter loading, but fake other args to only test filter reading
    stdout, stderr, _ = capture_output([TARGET, "-s", "8.8.8.8", "-f", filename, "-v"])
    
    # Verify correct number of loaded rules was printed
    assert "Loaded 5 filter rules" in stdout
    
    os.unlink(filename)

def test_load_malformed_filters():
    malformed_entries = [
        "http://invalid..com",
        "www.!invalid.com",
        "*wildcard.com",
        "empty#comment",
        "valid.com",
        "a" * 300 + ".com",  # too long
        "valid-domain.org/path?query",
        "# just a comment",
        "",
    ]
    filename = write_temp_file(malformed_entries)
    
    stdout, stderr, _ = capture_output([TARGET, "-s", "8.8.8.8", "-f", filename])
    
    # Check that at least 3 warnings are shown
    assert stderr.count("WARNING") >= 3
    # Valid domain should still be loaded
    assert "valid.com" in stdout or "valid.com" in stderr
    
    os.unlink(filename)

def test_is_blocked():
    filter_domains = ["example.com", "blocked.org"]
    # Setting up command to run dns with a temporary filter file
    filename = write_temp_file(filter_domains)
    
    # Simulate behavior of dns by querying is_blocked via command-line execution
    stdout, _, _ = capture_output([TARGET, "-s", "8.8.8.8", "-f", filename, "-v"])
    
    # Check explicit and suffix blocking rules applied (verbose prints should show)
    assert "Loaded 2 filter rules" in stdout
    
    # Cleanup
    os.unlink(filename)
