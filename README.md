# machine-vision-homework
机器视觉课程作业仓库，按「每次作业一个文件夹」的组织方式管理。

姓名：林旺铭
学号：24041827
班级：24186011
选课时间：周五下午6、7节

## 目录约定

```
machine vision/            ← 仓库根目录（.git 在这一层，Git 只认这一个仓库）
├── README.md              ← 本文件：学生信息 + 作业索引 + 操作流程
├── .gitignore             ← 全仓库唯一的忽略规则，对所有 homeworkN 生效
├── homework1/             ← 第一次作业：一个独立的 CMake 工程
│   ├── CMakeLists.txt
│   ├── src/main.cpp
│   ├── images/            ← 输入图
│   ├── results/           ← 结果图（要交的图放这里）
│   └── README.md          ← 该次作业的实验报告
└── homework2/             ← 第二次作业（结构同 homework1）
```

哪些放仓库根、哪些放作业文件夹里：

| 放仓库根目录 | 放 homeworkN 文件夹里 |
| --- | --- |
| `README.md`（总索引、学生信息） | `src/` 源码 |
| `.gitignore`（**全仓库只放这一份**） | `CMakeLists.txt` 等工程文件 |
| `homework1/`、`homework2/` 这些作业文件夹 | `images/` 输入图片 |
| — | `results/` 结果图 |
| — | `.vscode/` 本机编辑器配置（不入库） |
| — | `build/`、`output/` 编译与运行产物（不入库） |

## 作业索引

| 编号 | 文件夹 | 内容 | 状态 |
| --- | --- | --- | --- |
| 实验 01 | [homework1](homework1/README.md) | RGB 图像像素操作（OpenCV C++） | 已完成 |
| 实验 02 | [homework2](homework2/README.md) | 待完成 | 进行中 |

## 每次作业的操作流程

1. 在仓库根目录新建 `homeworkN` 文件夹；
2. VS Code 里「文件 → 打开文件夹」选择 `homeworkN`（**打开作业文件夹，不要打开仓库根**，CMake 工程才认得到）；
3. 写代码 → `Ctrl+S` 保存 → `Ctrl+Shift+B` 编译 → `F5` 调试；
4. 左侧「源代码管理」(`Ctrl+Shift+G`)：点文件右侧的 **+** 暂存改动 → 上方输入框写提交说明 → 点「提交」；
5. 点源代码管理右上角 **···** → **Push**（Git 按整个仓库推送，不是按文件夹）；
6. 刷新 GitHub 页面即可看到更新。

## 常用命令（在 homeworkN 目录下执行）

```powershell
# 配置 + 编译
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DOpenCV_DIR=D:/OpenCV_5.0.0/opencv/build
cmake --build build --config Release

# 查看该次作业改了什么再提交
git status
git diff
```
