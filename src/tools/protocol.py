from dataclasses import dataclass
from datetime import datetime
import zlib
import serial
import serial.tools.list_ports
import time

# Constants
CHUNK_SIZE = 1024
EOT = b'\x04'

@dataclass
class ProtocolFileInfo:
    name: str
    type: str
    size: int

@dataclass
class ProtocolInfo:
    device_name: str
    git_commit_sha: str
    protocol_version: str
    build_date: datetime
    fs_size: int
    free_space: int
    uses_eternity: bool
    debug_mode: bool

@dataclass
class MeasuredColor:
    hue: int
    saturation: int
    value: int
    clear: int

def find_serial_port(target_vid: int, target_pid: int):
    """
    Scan available serial ports and return the device name of the first port
    whose USB VID/PID match the targets. Returns None if not found.
    """
    ports = serial.tools.list_ports.comports()
    for port in ports:
        # On some systems port.vid/pid may be None
        if port.vid is None or port.pid is None:
            continue
        if (port.vid, port.pid) == (target_vid, target_pid):
            return port.device
    return None

# CRC32 helper
def crc32(data, crc=0):
    return zlib.crc32(data, crc) & 0xFFFFFFFF

class ProtocolClient:
    def __init__(self, baudrate=115200):
        port = find_serial_port(0xCAFE, 0x6942)
        if port is None:
            raise ConnectionRefusedError("No Sisyphus device found")
        self.serial = serial.Serial(port, baudrate, timeout=1)

    def send_command(self, cmd):
        if isinstance(cmd, str):
            cmd = cmd.encode('ascii')
        self.serial.write(cmd + EOT)
        self.serial.flush()

    def readline(self):
        line = self.serial.readline()
        return line.decode(errors='ignore').strip()

    def ls(self):
        """List remote files to an array"""

        self.send_command('ls')

        data = bytearray()
        while True:
            byte = self.serial.read(1)
            if byte == EOT or not byte:
                break
            data.extend(byte)
        text = data.decode('ascii', errors='ignore')

        files = text.splitlines()[2:] # Remove . and ..

        file_list = []

        for file in files:
            name, type, size = file.split()
            file_list.append(ProtocolFileInfo(name, type, int(size)))

        return file_list

    def rm(self, filename):
        """Remove a remote file"""

        self.send_command(f"rm {filename}")
        resp = self.readline()
        return resp.startswith('ack'), resp

    def mv(self, old, new):
        """Move a remote file"""

        self.send_command(f"mv {old} {new}")
        resp = self.readline()
        return resp.startswith('ack'), resp

    def push(self, file, size, dest, progress_cb=None):
        """Push a local file-like object to the remote device"""

        checksum = crc32(file.read())
        file.seek(0) # Reset to start of file

        self.send_command(f"push {dest} {size} {checksum}")

        if progress_cb is not None:
            start = time.time()
        sent = 0
        while sent < size:
            resp = self.readline()
            if not resp or not resp.startswith('ack'):
                return False, resp

            chunk = file.read(CHUNK_SIZE)
            self.serial.write(chunk)
            sent += len(chunk)

            if progress_cb is not None:
                # Show transfer speed
                elapsed = time.time() - start
                speed = sent / elapsed / 1024  # in KB/s
                progress_cb(sent, speed)

        resp = self.readline()
        return resp.startswith('ack'), resp

    def pull(self, remote, file, progress_cb=None, size_cb=None):
        """Pull a remote file from the device and write to a file-like object"""
        self.send_command(f"pull {remote}")

        resp = self.readline()
        if not resp or not resp.startswith('ack'):
            return False, resp

        resp = resp.split(" ")
        size, expected_checksum = map(int, resp[1:3])

        if size_cb is not None:
            size_cb(size)

        if progress_cb is not None:
            start = time.time()
        data = bytearray();
        while len(data) < size:
            self.send_command('ack')
            chunk = self.serial.read(min(CHUNK_SIZE, size - len(data)))
            data.extend(chunk)

            if progress_cb is not None:
                # Show transfer speed
                elapsed = time.time() - start
                speed = len(data) / elapsed / 1024  # in KB/s
                progress_cb(len(data), speed)

        checksum = crc32(data)
        if checksum != expected_checksum:
            return False, f"Checksum mismatch! Expected {expected_checksum}, got {checksum}"
        else:
            file.write(data)
            return True, ""

    def play(self, filename):
        """Play a remote file"""

        self.send_command(f"play {filename}")
        resp = self.readline()
        return resp.startswith('ack'), resp

    def info(self):
        """Get information about the device"""

        self.send_command("info")
        data = self.readline().split(" ")
        if data[0] != "sisyphus":
            raise ValueError("Unexpected response from device")
        build_date, block_count, fs_size, block_size, use_eternity, debug_mode = data[4:]
        parsed_data = data[1:4]
        return ProtocolInfo(*parsed_data, build_date = datetime.strptime(build_date, "%Y-%m-%d,%H:%M:%S"), fs_size = int(block_count)*int(block_size), free_space = int(fs_size)*int(block_size), uses_eternity = bool(int(use_eternity)), debug_mode = bool(int(debug_mode)))

    def measure(self):
        """Get a color measurement from the device's color sensor"""

        self.send_command("measure")
        data = self.readline().split(" ")
        hue, saturation, value, clear = data
        return MeasuredColor(int(hue), int(saturation), int(value), int(clear))

    def reset(self):
        """Reset the device to bootloader - BREAKS THE CONNECTION"""
        self.send_command("reset")
        time.sleep(0.01)
        self.serial.close() # Close the serial connection
        del(self) # Delete itself
