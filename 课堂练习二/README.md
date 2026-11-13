# 课堂练习二：硬币计数

**日期：2026 年 11 月 13 日（周五）**

用 OpenCV（C++）对一张暗背景上的硬币照片做自动计数。

## 任务

输入一张硬币散落的照片，输出硬币的枚数，并在图上标出每一个被计数的目标。

本次题目图片：`images/coins.jpg`（691 × 572），**实测结果 10 枚硬币**。

## 环境

| 项目 | 版本/位置 |
| --- | --- |
| 系统 | Windows 10/11 x64 |
| 编译器 | Visual Studio 2022（MSVC，x64） |
| OpenCV | 5.0.0（`D:\OpenCV_5.0.0\opencv\build`，vc16 库） |
| 构建 | CMake 3.29.5 |
| 编辑器 | VS Code |

## 目录结构

```
课堂练习二/
├── CMakeLists.txt        # 工程定义（链接 OpenCV、复制 dll）
├── src/main.cpp          # 计数算法：预处理 + 圆形筛选 + 标注输出
├── images/coins.jpg      # 题目输入图
├── results/              # 输出：
│   ├── coins_counted.png # 标注图（绿色轮廓 + 编号 + count）
│   ├── coins_binary.png  # 二值图
│   └── coins_gray.png    # 灰度图（--debug 时输出）
├── run.bat               # 双击一键运行
├── build/                # CMake 生成目录（已忽略，不入库）
└── .vscode/              # 本机 VS Code 配置（已忽略，不入库）
```

## 算法流程

1. **读图**：二进制流 + `imdecode`，中文路径也能读；
2. **灰度 + 高斯平滑**：`GaussianBlur(5×5)`，压掉背景噪点；
3. **Otsu 二值化（正向）**：本题是暗背景上的亮物体，和回形针那道题（`THRESH_BINARY_INV`）方向相反，硬币直接成为白色前景，自动求出阈值 **133**；
4. **形态学闭运算**（椭圆核 7×7）：填补硬币表面高光、浮雕纹理造成的细小黑洞，避免一枚硬币被拆成几块；
5. **轮廓提取**：`findContours(RETR_CCOMP)`，只取最外层轮廓；
6. **三条筛选规则**：

   | 规则 | 阈值 | 作用 |
   | --- | --- | --- |
   | 面积区间 | 1185 ~ 47430 px（占全图 0.3%~12%） | 排除噪点与整片背景 |
   | 圆度 `4πA/P²` | ≥ 0.55 | 硬币接近圆，排除长条状的边框线 |
   | 不贴图像边界 | 外接框距边界 > 8 px | 排除照片四周的亮边线 |

7. **标注输出**：绿色描边 + 橙色外接圆 + 编号 + 底部 `count = N`。

## 关键代码

```cpp
// 正向 Otsu：亮的硬币 -> 白色前景
double otsuValue = cv::threshold(blurred, binary, 0, 255,
                                 cv::THRESH_BINARY | cv::THRESH_OTSU);

// 闭运算填补硬币表面纹理造成的孔洞
cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(7, 7));
cv::morphologyEx(binary, cleaned, cv::MORPH_CLOSE, kernel);

// 圆度判别：正圆为 1，越细长越小
double circularity(const std::vector<cv::Point>& contour, double area) {
    double perimeter = cv::arcLength(contour, true);
    return 4.0 * CV_PI * area / (perimeter * perimeter);
}
```

## 运行结果

10 枚硬币全部命中，圆度都在 0.89~0.90（说明轮廓完整、没有被纹理撕碎）：

| 编号 | 面积(px) | 圆度 | 外接框(w×h) | 长宽比 | 中心坐标 |
| --- | --- | --- | --- | --- | --- |
| 1 | 15029 | 0.89 | 142×137 | 1.04 | (275,486) |
| 2 | 10289 | 0.90 | 118×112 | 1.05 | (547,403) |
| 3 | 14556 | 0.90 | 140×134 | 1.04 | (219,339) |
| 4 | 14108 | 0.89 | 138×132 | 1.05 | (403,278) |
| 5 | 10162 | 0.90 | 118×112 | 1.05 | (617,237) |
| 6 | 14398 | 0.89 | 140×133 | 1.05 | (80,246) |
| 7 | 9694 | 0.89 | 115×108 | 1.06 | (251,196) |
| 8 | 13842 | 0.90 | 137×130 | 1.05 | (501,162) |
| 9 | 10002 | 0.90 | 117×110 | 1.06 | (124,112) |
| 10 | 13649 | 0.90 | 136×129 | 1.05 | (342,76) |

面积分两档（约 1.0 万 与 1.4 万）对应两种大小的硬币，长宽比全部接近 1.05，符合圆形。

丢弃统计：内层轮廓 1（硬币内部区域，不能重复计数），无面积/形状/贴边误杀。

标注结果见 `results/coins_counted.png`：10 枚硬币逐个编号，无漏标、无重复。

## 编译与运行

VS Code：打开 `课堂练习二` 文件夹，`Ctrl+Shift+B` 编译；任务 `run: coin_count` 直接跑图。

命令行（当前目录为 `课堂练习二`）：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DOpenCV_DIR=D:/OpenCV_5.0.0/opencv/build
cmake --build build --config Release

build\Release\coin_count.exe images/coins.jpg --out results            # 带窗口
build\Release\coin_count.exe images/coins.jpg --out results --headless --debug   # 只算 + 诊断
```

或直接双击 `run.bat`。

参数可调（应对光照不同的照片）：`--min-area-ratio`、`--max-area-ratio`、`--min-circularity`、`--max-aspect`、`--margin`。

## 遇到的坑

- **阈值方向和第二题相反**：回形针是亮背景暗物体（`THRESH_BINARY_INV`），硬币是暗背景亮物体（`THRESH_BINARY`），直接照搬上一题的代码会把背景当成目标；
- **硬币表面会产生黑洞**：浮雕和高光让二值图里出现细小孔洞，若不闭合，一枚硬币可能被拆成多块或圆度明显下降；
- **照片四周有亮边线**：这条线面积小但极细长，靠「圆度 + 贴边」两条规则才排掉；
- **OpenCV 5.0 头文件位置**：`contourArea` / `boundingRect` / `arcLength` / `findContours` 在 `opencv2/geometry/2d.hpp`，只 include `opencv2.hpp` 会报 C3861；
- **控制台中文乱码**：MSVC 控制台默认按 GBK 解码，程序输出 UTF-8，需要在 `main` 开头 `SetConsoleOutputCP(CP_UTF8)`；
  且 `windows.h` 之前必须定义 `NOMINMAX`，否则它的 `min`/`max` 宏会把 `std::min` / `std::max` 破坏掉。
