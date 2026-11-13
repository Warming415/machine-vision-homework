# 课堂练习一：回形针计数

**日期：2026 年 11 月 13 日（周五）**

在课堂练习要求下，用 OpenCV（C++）对一张回形针散落照片做自动计数。

## 任务

输入一张回形针散落的照片，输出回形针的个数，并在图上标出每一个被计数的目标。

本次题目图片：`images/paperclips.jpg`（802 × 796），**实测结果 13 个回形针**。

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
课堂练习一/
├── CMakeLists.txt          # 工程定义（链接 OpenCV、复制 dll）
├── src/main.cpp            # 计数算法：预处理 + 轮廓筛选 + 标注输出
├── images/paperclips.jpg   # 题目输入图
├── results/                # 输出：
│   ├── paperclips_counted.png   # 标注图（绿轮廓 + 编号 + count）
│   ├── paperclips_binary.png    # 二值图（钢丝前景）
│   └── paperclips_gray.png      # 灰度图（--debug 时输出）
├── build/                  # CMake 生成目录（已忽略，不入库）
└── .vscode/                # 本机 VS Code 配置（已忽略，不入库）
```

## 算法流程

1. **读图**：二进制流 + `imdecode`，中文路径也能读；
2. **灰度 + 高斯平滑**：`cvtColor` → `GaussianBlur(5×5)`，压掉纸面纹理；
3. **Otsu 反向二值化**：回形针钢丝比背景暗，反向阈值让钢丝变成白色前景，
   自动求出阈值 **118**（灰度量程 0~255）；
4. **形态学闭运算**（椭圆核 5×5）：修补钢丝二值化后的细小断裂，使每个回形针成为连通的整体；
5. **轮廓提取**：`findContours(RETR_CCOMP)`，同时拿到外轮廓和内部的孔洞层次；
6. **四条筛选规则**（缺一条都会多计或漏计）：
   | 规则 | 阈值 | 作用 |
   | --- | --- | --- |
   | 面积区间 | 957 ~ 19151 px（占全图 0.15%~3%） | 排除噪点与大片背景 |
   | 长宽比 | ≤ 5.0 | 排除照片边框那种细长直线 |
   | 不贴图像边界 | 外接框距边界 > 6 px | 排除照片四周的深色边框线 |
   | 含封闭内孔 | 孔洞数 ≥ 1 | 回形针钢丝必然围出内孔，排除实心黑块 |
7. **标注输出**：绿色描边 + 序号 + 底部 `count = N`。

## 关键代码

```cpp
// Otsu 反向阈值：暗的钢丝 -> 白色前景
double otsuValue = cv::threshold(blurred, binary, 0, 255,
                                 cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

// 闭运算修补钢丝断点
cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
cv::morphologyEx(binary, cleaned, cv::MORPH_CLOSE, kernel);

// 外轮廓 + 层次信息（用于统计内孔）
cv::findContours(cleaned, contours, hierarchy, cv::RETR_CCOMP, cv::CHAIN_APPROX_SIMPLE);
```

## 运行结果

13 个目标全部命中，面积高度一致（说明每一个都是完整的一个回形针，不是碎片）：

| 编号 | 面积(px) | 外接框(w×h) | 长宽比 | 内孔 | 中心坐标 |
| --- | --- | --- | --- | --- | --- |
| 1 | 10861 | 160×153 | 1.05 | 2 | (173,645) |
| 2 | 11343 | 179×140 | 1.28 | 2 | (566,638) |
| 3 | 10976 | 80×204 | 2.55 | 2 | (415,633) |
| 4 | 10904 | 193×100 | 1.93 | 2 | (258,526) |
| 5 | 11118 | 196×82 | 2.39 | 2 | (451,486) |
| 6 | 10966 | 200×77 | 2.60 | 2 | (133,413) |
| 7 | 10950 | 72×206 | 2.86 | 2 | (646,426) |
| 8 | 10712 | 101×194 | 1.92 | 2 | (329,391) |
| 9 | 11033 | 113×188 | 1.66 | 2 | (516,353) |
| 10 | 10850 | 189×105 | 1.80 | 2 | (235,263) |
| 11 | 11135 | 170×141 | 1.21 | 2 | (603,191) |
| 12 | 11092 | 171×139 | 1.23 | 2 | (422,161) |
| 13 | 10950 | 170×150 | 1.13 | 2 | (180,123) |

丢弃统计：面积越界 12（多为 1~40 px 的细线噪点）、贴边 1（照片左侧 246×796 的边框线）、内层轮廓 26（每个回形针内部钢丝围出的区域，属于同一个目标，不能重复计数）。

标注结果见 `results/paperclips_counted.png`：13 个回形针逐个绿框编号，无漏标、无重复。

## 编译与运行

VS Code：打开 `课堂练习一` 文件夹，按 `Ctrl+Shift+B` 编译；任务 `run: paperclip_count` 直接跑图。

命令行（当前目录为 `课堂练习一`）：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DOpenCV_DIR=D:/OpenCV_5.0.0/opencv/build
cmake --build build --config Release

# 带窗口（左右显示标注图与二值图，按任意键退出）
build\Release\paperclip_count.exe images/paperclips.jpg --out results

# 无窗口批处理，并打印每个目标的面积/外接框等诊断信息
build\Release\paperclip_count.exe images/paperclips.jpg --out results --headless --debug
```

参数可调（应对光照不同的照片）：`--min-area-ratio`、`--max-area-ratio`、`--max-aspect`、`--margin`、`--no-hole-check`。

## 遇到的坑

- **OpenCV 5.0 改了头文件位置**：`contourArea` / `boundingRect` / `findContours` 不再由 `imgproc.hpp` 提供，而是移到了
  `opencv2/geometry/2d.hpp`，不额外 `#include` 会报 C3861「找不到标识符」；
- **照片自带深色边框**：左边那条 246×796 的边框线本身是个面积 2551 的前景块，靠「贴边 + 长宽比」两条规则才排掉，
  否则会多算 1 个；
- **内层轮廓会重复计数**：`RETR_CCOMP` 下每个回形针还有 2 个内部区域轮廓（26 个），只取 `hierarchy[i][3] == -1`
  的最外层轮廓，否则会把 13 个算成 39 个；
- 阈值不能写死：本次 Otsu 自动取 118，若改用固定阈值，光照稍变就会漏检。
