import serial
import time

ser = serial.Serial('COM4',115200)

time.sleep(2)   # allow Arduino reset

ser.write(b'r') # start recording

file = open("benchmark.csv","w", buffering=1)

try:
    while True:
        line = ser.readline().decode(errors='ignore').strip()
        print(line)
        file.write(line + "\n")

except KeyboardInterrupt:
    print("Stopping recording")

finally:
    ser.write(b's')  # stop Arduino recording
    file.close()
    ser.close()