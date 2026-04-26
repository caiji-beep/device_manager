import cv2
import numpy as np
import os

def generate_c_array(color_bgr, var_name, filename):
    print(f"正在生成 {var_name} (640x480) ...")
    # 1. 创建 640x480 的纯色图像矩阵
    img = np.zeros((480, 640, 3), dtype=np.uint8)
    img[:] = color_bgr

    # 2. 编码为 JPEG 格式 (质量设为 80，既能看清体积又小)
    success, encoded = cv2.imencode('.jpg', img, [cv2.IMWRITE_JPEG_QUALITY, 80])
    if not success:
        print(f"❌ 编码 {var_name} 失败！")
        return

    # 3. 转换为 C 语言能认的十六进制数组 (格式：0xff)
    hex_list = [f"0x{b:02x}" for b in encoded.tobytes()]
    
    # 每行放 12 个十六进制数，让生成的 .h 文件排版美观
    lines = [", ".join(hex_list[i:i+12]) for i in range(0, len(hex_list), 12)]
    
    # 4. 写入文件
    with open(filename, "w") as f:
        f.write(f"// Automatically generated 640x480 MJPEG data\n")
        f.write(f"unsigned char {var_name}[{len(encoded)}] = {{\n")
        f.write(",\n".join(lines))
        f.write("\n};\n")
    print(f"✅ 成功生成文件: {filename}，数组大小: {len(encoded)} 字节\n")

if __name__ == "__main__":
    # 使用新的颜色，并给 C 变量起个新名字，防止和原来的 red/green/blue 冲突
    # BGR 格式：(蓝, 绿, 红)
    
    # 生成黄色 (Yellow: 绿+红)
    generate_c_array((0, 255, 255), 'pic_yellow', 'yellow_640x480.h')     
    
    # 生成青色 (Cyan: 蓝+绿)
    generate_c_array((255, 255, 0), 'pic_cyan', 'cyan_640x480.h') 
    
    # 生成洋红色 (Magenta: 蓝+红)
    generate_c_array((255, 0, 255), 'pic_magenta', 'magenta_640x480.h')