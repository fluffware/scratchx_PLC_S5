#! /usr/bin/python3
import serial;
import sys;

def skip_prompt(ser):
    while True:
        bytes = ser.read()
        if b'>' in bytes:
            break;
        
ser = serial.Serial(sys.argv[1], 38400, timeout=1)
ser.write(b"nop\n")
skip_prompt(ser)
for line in sys.stdin:
    if line[0] != ':':
        raise Exception("Line doesn't start with :")
    l = int(line[1:3],16)
    if (sum(bytes.fromhex(line[1:11+l*2])) & 0xff) != 0:
        raise Exception("Checksum error")
    addr = int(line[3:7],16)
    type = int(line[7:9])
    if type == 0:
        d = bytes.fromhex(line[9:9+l*2])
        cmd = "W"+format(addr,"x")+" "+" ".join(format(v,"x") for v in d)+"\n"
        ser.write(bytes(cmd,"ASCII"))
        skip_prompt(ser)
        
        

