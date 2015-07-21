#! python2.7
import serial;
import sys;
ser = serial.Serial(sys.argv[1], 9600)
start = 0
l = 16
if len(sys.argv) >= 3: start = int(sys.argv[2],0)
if len(sys.argv) >= 4: l = int(sys.argv[3],0)
ser.write("W%04x"%start);
for i in range(0,l):
    print(i)
    ser.write(" %02x" % (i/256))
    ser.write(" %02x" % (i%256))

ser.write("\n")

