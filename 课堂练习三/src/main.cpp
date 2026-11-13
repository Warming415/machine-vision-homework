// 课堂练习三 —— 完整圆孔检测与计数（OpenCV C++）
//
// 题目要求：检测图片中「完整」的圆孔并计数。
// 难点在于这张照片是金属件局部特写，图像四周本来就有被裁掉一半的孔（上边缘的小孔、
// 右侧的大孔、左上角的暗区），它们只有一部分落在画面里，属于「不完整」，不能计入。
//
// 处理流程：
//   1. 读图（二进制流 + imdecode，兼容中文路径）
//   2. 灰度 + 中值滤波（去金属表面的斑点噪声，中值比高斯更能保住圆的边缘）
//   3. HoughCircles 霍夫圆检测（梯度法），得到候选圆（圆心 + 半径）
//   4. 逐候选做两项判定：
//      a. 完整性：圆必须整个落在画面内（center±r 都在图像范围内，留 margin 像素）
//      b. 孔芯亮度：圆内平均灰度明显低于圆外环带（孔是暗的，金属是亮的），
//         用来排掉由同心圆环、反光边缘误检出的「假圆」
//   5. 通过判定的圆标注编号、圆内平均灰度、半径，输出结果图

// Windows 控制台默认按 GBK 解码，而本程序统一输出 UTF-8，
// 因此需要把控制台输出代码页切到 UTF-8，否则中文全是乱码。
// NOMINMAX 必须定义，否则 windows.h 的 min/max 宏会破坏 std::min / std::max。
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <opencv2/opencv.hpp>
// OpenCV 5.0 把轮廓几何函数挪到了 geometry 模块，这里一并包含，避免个别版本缺声明
#include <opencv2/geometry/2d.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct Options {
    std::string input = "images/holes.jpg";
    std::string outDir = "results";
    double dp = 1.0;              // 累加器分辨率（1 = 与输入同分辨率）
    double minDist = 40.0;        // 两个圆心之间的最小距离，避免同一个孔检出多个圆
    double param1 = 100.0;        // Canny 高阈值（内部低阈值为其一半）
    double param2 = 30.0;         // 累加器阈值：越小越灵敏、误检越多
    int minRadius = 15;           // 半径下限（像素）
    int maxRadius = 160;          // 半径上限（像素）
    int margin = 2;               // 圆离画面边缘至少留这么多像素才算「完整」
    double minContrast = 15.0;    // 圆内均值要比圆外环带暗多少才算孔
    double minEdgeSupport = 0.60; // 圆周上必须有边缘像素支撑的比例（排除暗区里拟合出的假圆）
    bool headless = false;
    bool debug = false;
};

void printUsage() {
    std::cout <<
        "用法: hole_count [图片路径] [选项]\n"
        "  默认图片: images/holes.jpg\n"
        "选项:\n"
        "  --out <目录>        结果输出目录（默认 results）\n"
        "  --headless          算完直接退出，不显示窗口\n"
        "  --debug             额外保存 Canny 边缘图，并打印被排除的候选\n"
        "  --param2 <v>        霍夫累加器阈值（默认 30，越小越灵敏）\n"
        "  --param1 <v>        Canny 高阈值（默认 100）\n"
        "  --min-dist <v>      圆心最小间距（默认 40）\n"
        "  --min-radius <v>    半径下限（默认 15）\n"
        "  --max-radius <v>    半径上限（默认 160）\n"
        "  --margin <v>        完整性判定的边缘余量（默认 2）\n"
        "  --min-contrast <v>  孔内/孔外亮度差下限（默认 15）\n"
        "  --min-edge-support <v>  圆周边缘支撑度下限（默认 0.60）\n";
}

cv::Mat readImage(const std::string& path, int flags = cv::IMREAD_COLOR) {
    std::ifstream in(fs::path(path), std::ios::binary);
    if (!in) return {};
    std::vector<uchar> buf((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (buf.empty()) return {};
    return cv::imdecode(buf, flags);
}

bool writeImage(const std::string& path, const cv::Mat& img) {
    std::vector<uchar> buf;
    std::vector<int> params{cv::IMWRITE_PNG_COMPRESSION, 3};
    if (!cv::imencode(".png", img, buf, params)) return false;
    std::ofstream out(fs::path(path), std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    return out.good();
}

// 圆内平均灰度（只取半径 0.75r 的同心圆，躲开模糊的边缘带）
double meanInside(const cv::Mat& gray, cv::Point2f c, float r) {
    cv::Mat mask = cv::Mat::zeros(gray.size(), CV_8U);
    cv::circle(mask, c, std::max(2, static_cast<int>(r * 0.75)), 255, cv::FILLED);
    return cv::mean(gray, mask)[0];
}

// 圆外环带平均灰度（1.25r ~ 1.7r），代表孔周围的金属亮度
double meanOutside(const cv::Mat& gray, cv::Point2f c, float r) {
    cv::Mat mask = cv::Mat::zeros(gray.size(), CV_8U);
    cv::circle(mask, c, std::max(3, static_cast<int>(r * 1.7)), 255, cv::FILLED);
    cv::circle(mask, c, std::max(2, static_cast<int>(r * 1.25)), 0, cv::FILLED);
    return cv::mean(gray, mask)[0];
}

// 圆周边缘支撑度：沿圆周每隔 3° 取一个点，看该点附近（半径 r-2 ~ r+2）是否有 Canny 边缘。
// 真正的孔一定有一整圈清晰的孔壁，暗区里拟合出来的「假圆」则大部分圆周上没有边缘。
double edgeSupport(const cv::Mat& edges, cv::Point2f c, float r) {
    int hit = 0, total = 0;
    for (int ang = 0; ang < 360; ang += 3) {
        double rad = ang * CV_PI / 180.0;
        double dx = std::cos(rad), dy = std::sin(rad);
        bool found = false;
        for (int dr = -2; dr <= 2 && !found; ++dr) {
            int x = static_cast<int>(std::lround(c.x + (r + dr) * dx));
            int y = static_cast<int>(std::lround(c.y + (r + dr) * dy));
            if (x < 0 || y < 0 || x >= edges.cols || y >= edges.rows) continue;
            if (edges.at<uchar>(y, x) != 0) found = true;
        }
        ++total;
        if (found) ++hit;
    }
    return total > 0 ? static_cast<double>(hit) / total : 0.0;
}

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);   // 控制台按 UTF-8 显示，避免中文乱码
#endif
    Options opt;
    std::string inputGiven;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") { printUsage(); return 0; }
        else if (a == "--out" && i + 1 < argc) opt.outDir = argv[++i];
        else if (a == "--headless") opt.headless = true;
        else if (a == "--debug") opt.debug = true;
        else if (a == "--dp" && i + 1 < argc) opt.dp = std::stod(argv[++i]);
        else if (a == "--param1" && i + 1 < argc) opt.param1 = std::stod(argv[++i]);
        else if (a == "--param2" && i + 1 < argc) opt.param2 = std::stod(argv[++i]);
        else if (a == "--min-dist" && i + 1 < argc) opt.minDist = std::stod(argv[++i]);
        else if (a == "--min-radius" && i + 1 < argc) opt.minRadius = std::stoi(argv[++i]);
        else if (a == "--max-radius" && i + 1 < argc) opt.maxRadius = std::stoi(argv[++i]);
        else if (a == "--margin" && i + 1 < argc) opt.margin = std::stoi(argv[++i]);
        else if (a == "--min-contrast" && i + 1 < argc) opt.minContrast = std::stod(argv[++i]);
        else if (a == "--min-edge-support" && i + 1 < argc) opt.minEdgeSupport = std::stod(argv[++i]);
        else if (a.size() > 2 && a.rfind("--", 0) == 0) {
            std::cerr << "未知选项: " << a << "\n";
            printUsage();
            return 2;
        }
        else inputGiven = a;
    }
    if (!inputGiven.empty()) opt.input = inputGiven;

    cv::Mat src = readImage(opt.input);
    if (src.empty()) {
        std::cerr << "读图失败: " << opt.input << "\n";
        return 1;
    }
    std::cout << "输入图片: " << opt.input << "\n";
    std::cout << "图像尺寸: " << src.cols << " x " << src.rows
              << "，通道 " << src.channels() << "\n";

    // 1) 灰度 + 中值滤波
    cv::Mat gray, blurred;
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    cv::medianBlur(gray, blurred, 5);

    // 2) 霍夫圆检测
    std::vector<cv::Vec3f> circles;
    cv::HoughCircles(blurred, circles, cv::HOUGH_GRADIENT,
                     opt.dp, opt.minDist, opt.param1, opt.param2,
                     opt.minRadius, opt.maxRadius);

    std::cout << "霍夫候选圆: " << circles.size() << " 个"
              << "（param1=" << opt.param1 << "，param2=" << opt.param2 << "）\n\n";

    // 圆周边缘支撑度用的 Canny 边缘图
    cv::Mat edges;
    cv::Canny(blurred, edges, opt.param1 / 2.0, opt.param1);

    std::vector<cv::Vec3f> kept;
    int rejectIncomplete = 0, rejectBright = 0, rejectNoEdge = 0;

    for (size_t i = 0; i < circles.size(); ++i) {
        cv::Point2f c(circles[i][0], circles[i][1]);
        float r = circles[i][2];

        // a. 完整性：整个圆必须落在画面内
        bool complete = (c.x - r >= opt.margin) && (c.y - r >= opt.margin) &&
                        (c.x + r <= src.cols - 1 - opt.margin) &&
                        (c.y + r <= src.rows - 1 - opt.margin);

        double inMean = meanInside(gray, c, r);
        double outMean = meanOutside(gray, c, r);
        double contrast = outMean - inMean;
        bool isDark = contrast >= opt.minContrast;
        double support = edgeSupport(edges, c, r);
        bool hasEdge = support >= opt.minEdgeSupport;

        if (complete && isDark && hasEdge) {
            kept.push_back(circles[i]);
            std::cout << std::left << std::setw(4) << ("#" + std::to_string(kept.size()))
                      << " 圆心(" << std::right << std::setw(4) << static_cast<int>(c.x)
                      << "," << std::setw(4) << static_cast<int>(c.y) << ")"
                      << "  半径 " << std::setw(4) << static_cast<int>(r)
                      << "  孔内均值 " << std::fixed << std::setprecision(1) << std::setw(6) << inMean
                      << "  孔外均值 " << std::setw(6) << outMean
                      << "  亮度差 " << std::setw(5) << contrast
                      << "  边缘支撑 " << std::setw(4) << std::setprecision(2) << support << "\n";
        } else {
            if (!complete) ++rejectIncomplete;
            if (!isDark) ++rejectBright;
            if (!hasEdge) ++rejectNoEdge;
            if (opt.debug) {
                std::cout << "    排除 圆心(" << static_cast<int>(c.x) << ","
                          << static_cast<int>(c.y) << ") 半径 " << static_cast<int>(r)
                          << " 孔内 " << std::setprecision(1) << inMean
                          << " 孔外 " << outMean
                          << " 支撑 " << std::setprecision(2) << support
                          << " -> " << (!complete ? "圆被画面边缘截断" : "")
                          << (!complete && (!isDark || !hasEdge) ? " + " : "")
                          << (!isDark ? "圆内不够暗(不是孔)" : "")
                          << (!isDark && !hasEdge ? " + " : "")
                          << (!hasEdge ? "圆周无边缘(假圆)" : "") << "\n";
            }
        }
    }

    std::cout << "\n=== 完整圆孔计数: " << kept.size() << " 个 ===\n";
    std::cout << "排除统计: 被边缘截断 " << rejectIncomplete
              << "，圆内不够暗 " << rejectBright
              << "，圆周无边缘 " << rejectNoEdge << "\n";

    // 3) 标注输出
    cv::Mat canvas = src.clone();
    for (size_t k = 0; k < kept.size(); ++k) {
        cv::Point2f c(kept[k][0], kept[k][1]);
        float r = kept[k][2];
        cv::circle(canvas, c, static_cast<int>(r), cv::Scalar(0, 220, 0), 2, cv::LINE_AA);
        cv::circle(canvas, c, 2, cv::Scalar(0, 220, 0), cv::FILLED, cv::LINE_AA);
        cv::putText(canvas, std::to_string(k + 1),
                    cv::Point(static_cast<int>(c.x - r) - 4, static_cast<int>(c.y - r) - 6),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 0, 255), 2, cv::LINE_AA);
    }
    cv::putText(canvas, "complete holes = " + std::to_string(kept.size()),
                cv::Point(14, src.rows - 16), cv::FONT_HERSHEY_SIMPLEX, 0.9,
                cv::Scalar(0, 0, 255), 2, cv::LINE_AA);

    fs::create_directories(fs::path(opt.outDir));
    std::string outCounted = (fs::path(opt.outDir) / "holes_counted.png").string();
    bool ok1 = writeImage(outCounted, canvas);
    std::cout << (ok1 ? "[保存] " : "[失败] ") << outCounted << "\n";

    if (opt.debug) {
        writeImage((fs::path(opt.outDir) / "holes_edges.png").string(), edges);
        writeImage((fs::path(opt.outDir) / "holes_gray.png").string(), gray);
    }

    if (!opt.headless) {
        cv::imshow("complete holes", canvas);
        std::cout << "\n按任意键退出（ESC 也可）...\n";
        cv::waitKey(0);
    }
    return 0;
}
