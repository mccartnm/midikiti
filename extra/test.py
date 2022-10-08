import serial
import struct
import time

midikiti = serial.Serial(port='COM8', baudrate=9600, timeout=.1)



def get_layout():

    data = bytes()
    while arduino.in_waiting:
        data += arduino.read()

    print (data)

    # StartFlag | GetLayout
    header = struct.pack('BBB', 0xFF, 0x01, 0x01)
    arduino.write(header)

    body = struct.pack('B', 0x0)
    arduino.write(body)

    time.sleep(0.04)

    data = bytes()
    while arduino.in_waiting:
        data += arduino.read()

    print (data)

    # StartFlag | GetLayout | count
    # get_layout_header = struct.unpack('BBB', data)

    # print (get_layout_header)

get_layout()