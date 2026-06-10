import serial
import time

# ================= 配置参数 =================
# 请根据 ls /dev/ttyUSB* 的实际结果修改端口号
SERIAL_PORT = '/dev/ttyUSB0' 
BAUD_RATE = 115200  # 波特率，需与你连接的单片机/设备保持一致
TIMEOUT = 1       # 读取超时时间(秒)
# ============================================

def main():
    try:
        # 初始化并打开串口
        print(f"正在尝试打开串口: {SERIAL_PORT}，波特率: {BAUD_RATE}...")
        ser = serial.Serial(
            port=SERIAL_PORT,
            baudrate=BAUD_RATE,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=TIMEOUT
        )
        
        if ser.is_open:
            print("串口打开成功！")
            
            # 1. 发送数据示例
            send_data = "Hello CH340!\r\n"
            ser.write(send_data.encode('utf-8')) # 必须编码为字节流(bytes)
            print(f"已发送数据: {send_data.strip()}")
            
            # 2. 循环接收数据
            print("开始监听接收数据（按 Ctrl+C 退出）...\n")
            while True:
                # 检查缓冲区是否有可读数据
                if ser.in_waiting > 0:
                    # 读取一行数据（以 \n 结尾）或者直到超时
                    raw_data = ser.readline()
                    
                    try:
                        # 尝试将字节解码为字符串显示
                        decoded_data = raw_data.decode('utf-8').strip()
                        print(f"[收到文本]: {decoded_data}")
                    except UnicodeDecodeError:
                        # 如果接收的是十六进制等非文本数据，则直接打印十六进制
                        print(f"[收到十六进制]: {raw_data.hex()}")
                        
                time.sleep(0.1) # 稍作停顿，降低 CPU 占用

    except serial.SerialException as e:
        print(f"串口错误: {e}")
    except KeyboardInterrupt:
        print("\n用户终止程序。")
    finally:
        # 确保程序退出时关闭串口
        if 'ser' in locals() and ser.is_open:
            ser.close()
            print("串口已安全关闭。")

if __name__ == '__main__':
    main()