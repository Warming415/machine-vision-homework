# 课堂练习三：完整圆孔检测与计数

**日期：2026 年 11 月 13 日（周五）**

用 OpenCV（C++）检测金属件照片中**完整**的圆孔并计数。

## 任务

输入一张金属件局部特写照片，检测画面中完整的圆孔并给出个数。

本次题目图片：`images/holes.jpg`（818 × 614），**实测结果 4 个完整圆孔**。

「完整」是关键约束：这张照片是零件的局部特写，画面四周本来就有被裁掉一半的孔——
上边缘被切掉的小孔、右侧被切掉的大孔、左上和左下两片被裁掉的暗区。它们只有一部分落在画面里，
**属于不完整，不能计入**。

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
课堂练习三/
├── CMakeLists.txt        # 工程定义（链接 OpenCV、复制 dll）
├── src/main.cpp          # 检测算法：霍夫圆 + 三重校验 + 标注输出
├── images/holes.jpg      # 题目输入图
├── results/              # 输出：
│   ├── holes_counted.png # 标注图（绿圈 + 编号 + complete holes = N）
│   ├── holes_edges.png   # Canny 边缘图（--debug 时输出）
│   └── holes_gray.png    # 灰度图（--debug 时输出）
├── run.bat               # 双击一键运行
├── build/                # CMake 生成目录（已忽略，不入库）
└── .vscode/              # 本机 VS Code 配置（已忽略，不入库）
```

## 算法流程

1. **读图**：二进制流 + `imdecode`，中文路径也能读；
2. **灰度 + 中值滤波**（5×5）：金属表面有斑点噪声，中值滤波比高斯更能保住圆的边缘；
3. **`HoughCircles` 霍夫圆检测**（`HOUGH_GRADIENT`）：直接得到圆心 + 半径的候选圆，
   比先二值化再找轮廓更适合本图——因为金属件本身有大片明暗过渡，阈值法会把它们连成一片；
4. **三重校验**，滤掉不符合「完整圆孔」定义的候选：

   | 校验 | 判据 | 作用 |
   | --- | --- | --- |
   | 完整性 | 圆心 ± 半径 全部落在画面内（留 2px 余量） | 排除被边缘截断的孔：右侧大孔、上边缘小孔 |
   | 孔芯亮度 | 孔内平均灰度比孔外环带暗 ≥ 15 | 排除亮处的假圆（同心圆环、反光边缘） |
   | 圆周边缘支撑度 | 圆周上 ≥ 60% 的角度在半径 ±2px 内有 Canny 边缘 | 排除在暗区里拟合出的假圆——真孔有一整圈孔壁，暗区没有 |

5. **标注输出**：绿色圆圈 + 圆心 + 编号 + 底部 `complete holes = N`。

## 关键代码

```cpp
// 霍夫圆检测：直接给出圆心与半径
cv::HoughCircles(blurred, circles, cv::HOUGH_GRADIENT,
                 dp, minDist, param1, param2, minRadius, maxRadius);

// 完整性：整个圆必须落在画面内
bool complete = (c.x - r >= margin) && (c.y - r >= margin) &&
                (c.x + r <= cols - 1 - margin) && (c.y + r <= rows - 1 - margin);

// 圆周边缘支撑度：沿圆周每 3° 找一次边缘，命中比例 ≥ 60% 才算真圆
for (int ang = 0; ang < 360; ang += 3) { /* 在 r-2 ~ r+2 内查 Canny 边缘 */ }
```

## 运行结果（默认参数 param2 = 30）

| 编号 | 圆心 | 半径 | 孔内均值 | 孔外均值 | 亮度差 | 边缘支撑 |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | (460,147) | 42 | 86.5 | 146.6 | 60.1 | 0.92 |
| 2 | (584,490) | 42 | 89.1 | 183.5 | 94.4 | 0.97 |
| 3 | (466,339) | 24 | 86.3 | 162.4 | 76.1 | 0.97 |
| 4 | (767,544) | 24 | 73.0 | 177.8 | 104.7 | 0.97 |

4 个孔的边缘支撑度都在 0.92 以上，说明每个都是一整圈清晰的孔壁。

## 稳定性验证

把霍夫累加器阈值从 30 一路降到 15（越灵敏、误检越多），结论不变：

| param2 | 霍夫候选圆 | 最终计数 | 说明 |
| --- | --- | --- | --- |
| 30（默认） | 4 | **4** | 直接给出正确结果，无排除 |
| 20 | 17 | **4** | 多出 13 个误检，全被「截断/不够暗/无边缘」三条规则排除 |
| 15 | 39 | **4** | 多出 35 个误检，同样全部排除 |

也就是说结果不依赖单一阈值，三重校验把误检兜住了。

标注结果见 `results/holes_counted.png`：4 个完整孔逐个编号，被裁掉的大孔与小孔都没有计入。

## 编译与运行

VS Code：打开 `课堂练习三` 文件夹，`Ctrl+Shift+B` 编译；任务 `run: hole_count` 直接跑图。

命令行（当前目录为 `课堂练习三`）：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DOpenCV_DIR=D:/OpenCV_5.0.0/opencv/build
cmake --build build --config Release

build\Release\hole_count.exe images/holes.jpg --out results            # 带窗口
build\Release\hole_count.exe images/holes.jpg --out results --headless --debug   # 只算 + 诊断
```

或直接双击 `run.bat`。

可调参数：`--param2`（霍夫灵敏度）、`--param1`（Canny 高阈值）、`--min-radius` / `--max-radius`、
`--min-contrast`（孔内外的亮度差下限）、`--min-edge-support`（圆周边缘支撑度下限）、`--margin`（完整性余量）。

## 遇到的坑

- **阈值法在这里不可靠**：金属件有大片明暗过渡，Otsu 二值化会把孔和阴影连成一片，所以改用霍夫圆；
- **霍夫会把被裁掉的孔也检出来**：右侧大孔、上边缘小孔的圆心仍在画面内，必须用「圆心 ± 半径」判断完整性，
  只看圆心在不在画面里是不够的；
- **降阈值后大片暗区会拟合出假圆**（例如左侧暗区、大孔内部都检出了半径 100+ 的「圆」），
  单靠「孔内比孔外暗」过滤不掉（它们确实很暗），必须再加「圆周边缘支撑度」这一条；
- **OpenCV 5.0 头文件位置**：`contourArea` / `boundingRect` 等轮廓几何函数在 `opencv2/geometry/2d.hpp`；
- **控制台中文乱码**：MSVC 控制台默认 GBK，程序输出 UTF-8，需在 `main` 开头 `SetConsoleOutputCP(CP_UTF8)`，
  且 `windows.h` 之前要定义 `NOMINMAX`。
