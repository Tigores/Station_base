import cv2
import time

# 打开摄像头。通常，摄像头的ID是0，可能是1或其他值，取决于你的设备。
cap = cv2.VideoCapture(1)

if not cap.isOpened():
    print("无法打开摄像头")
    exit()

# 创建一个窗口并设置窗口大小
cv2.namedWindow('Camera Feed', cv2.WINDOW_NORMAL)
cv2.resizeWindow('Camera Feed', 1024, 512)

# 初始化帧率计数器
num_frames = 0
start_time = time.time()

while True:
    # 逐帧捕捉
    ret, frame = cap.read()

    # 如果帧读取错误，退出循环
    if not ret:
        print("无法接收帧 (stream end?). Exiting ...")
        break

    # 显示帧
    cv2.imshow('Camera Feed', frame)

    # 帧率计算
    num_frames += 1
    elapsed_time = time.time() - start_time
    if elapsed_time >= 1.0:
        fps = num_frames / elapsed_time
        print(f"FPS: {fps:.2f}")
        num_frames = 0
        start_time = time.time()

    # 按下 'q' 键退出循环
    if cv2.waitKey(1) == ord('q'):
        break

# 释放摄像头并关闭窗口
cap.release()
cv2.destroyAllWindows()
