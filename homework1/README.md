# machine-vision-homework
机器视觉课程作业，包含历次实验代码、结果图。
姓名：林旺铭
学号：24041827
班级：24186011
选课时间：周五下午6、7节

---

## 实验 01：RGB 图像像素操作

用 OpenCV（C++）逐像素访问彩色图像，把 B、G、R 三个通道的数值各除以 2，得到整体变暗的图像，用于理解 `Mat` 的三通道内存布局与 `at<Vec3b>` 的用法。

### 环境

| 项目 | 版本/位置 |
| --- | --- |
| 系统 | Windows 10/11 x64 |
| 编译器 | Visual Studio 2022（MSVC 19.41，x64） |
| OpenCV | 5.0.0（`D:\OpenCV_5.0.0\opencv\build`，vc16 库） |
| 构建 | CMake 3.29.5 |
| 编辑器 | VS Code |

### 目录结构

```
machine vision/
├── CMakeLists.txt          # 工程定义（自动链接 OpenCV、拷贝 dll）
├── src/main.cpp            # 源码：像素操作 + 批处理 + 结果输出
├── images/sample_rgb.png   # 测试图（六色块 + 渐变背景）
├── results/                # 运行结果图（变暗图、左右对比图）
├── build/                  # CMake 生成目录（已忽略，不入库）
├── output/                 # 默认输出目录（已忽略，不入库）
└── .vscode/                # 本机 VS Code 配置（已忽略，不入库）
```

### 编译

命令行：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DOpenCV_DIR=D:/OpenCV_5.0.0/opencv/build
cmake --build build --config Release
```

VS Code 中：直接按 `Ctrl+Shift+B`，执行默认任务 `cmake: build`（配置 + 编译一步完成）。

### 运行

```powershell
# 默认读取 D:\chiikawa 目录（该目录不存在时读取工程内 images/）
build\Release\rgb_pixel_demo.exe

# 处理指定图片
build\Release\rgb_pixel_demo.exe D:\chiikawa\001.png

# 处理整个文件夹，结果存到 results
build\Release\rgb_pixel_demo.exe D:\chiikawa --out results

# 无窗口批处理（只保存结果，不等待按键）
build\Release\rgb_pixel_demo.exe D:\chiikawa --out results --headless
```

带窗口运行时，程序左右并排显示「原图 | 变暗图」，按任意键切换下一张，按 ESC 退出。

### 核心代码

```cpp
Mat dst = src.clone();
for (int j = 0; j < dst.rows; ++j) {
    for (int i = 0; i < dst.cols; ++i) {
        Vec3b& px = dst.at<Vec3b>(j, i);          // Vec3b = 3 个 uchar
        px[0] = static_cast<uchar>(px[0] / 2);    // B
        px[1] = static_cast<uchar>(px[1] / 2);    // G
        px[2] = static_cast<uchar>(px[2] / 2);    // R
    }
}
```

几点说明：

- OpenCV 彩色图的通道顺序是 **BGR**，不是 RGB，所以 `[0]` 是蓝、`[1]` 是绿、`[2]` 是红；
- `at<Vec3b>(行, 列)` 的第一个参数是 **行（y）**，第二个是 **列（x）**，写反不会报错但结果错位；
- 像素是 `uchar`（0~255），除以 2 是整数除法，向下取整，不会溢出。

### 运行结果

测试图 `images/sample_rgb.png`（600×400）处理后：

| 采样点 | 原图像素 B/G/R | 处理后 B/G/R |
| --- | --- | --- |
| 红色块中心 | 60 / 60 / 250 | 30 / 30 / 125 |
| 绿色块中心 | 90 / 200 / 60 | 45 / 100 / 30 |
| 蓝色块中心 | 240 / 120 / 70 | 120 / 60 / 35 |
| 灰色块中心 | 128 / 128 / 128 | 64 / 64 / 64 |

全图平均亮度从 B/G/R = 226.3 / 199.1 / 192.9 降到 112.9 / 99.3 / 96.2，约为原来的一半，符合预期。

对比图见 `results/sample_rgb_compare.png`。

### 遇到的坑

- `imread` / `imwrite` 直接传含中文的路径会读图失败，本工程改用 `imdecode` / `imencode` 走二进制流，中文路径与中文文件名都能正常处理；
- 程序运行需要 `opencv_world500.dll`，`CMakeLists.txt` 已用 POST_BUILD 自动把它复制到 exe 同目录，不用手动改 PATH；
- 源码带中文注释，MSVC 必须在编译选项里加 `/utf-8`，否则会报 C4819 或中文乱码。
