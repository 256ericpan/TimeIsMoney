import serial
import time
from datetime import datetime
import sys
from serial.tools import list_ports

baudrate = 115200

# 获取所有可用串口
ports = [port.device for port in list_ports.comports()]
if not ports:
    print("未检测到任何串口设备！")
    sys.exit(1)

sers = []
for port in ports:
    try:
        ser = serial.Serial(port, baudrate, timeout=1)
        sers.append(ser)
        print(f"已连接到串口 {port}")
    except Exception as e:
        print(f"无法打开串口 {port}: {e}")

if not sers:
    print("没有可用串口可发送数据！")
    sys.exit(1)

print(f"每10秒向所有串口发送PC时间...")

try:
    while True:
        now = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        for ser in sers:
            try:
                ser.write((now + '\n').encode())
            except Exception as e:
                print(f"发送到 {ser.port} 失败: {e}")
        time.sleep(10)
except KeyboardInterrupt:
    print("\n已退出。")
finally:
    for ser in sers:
        ser.close() 