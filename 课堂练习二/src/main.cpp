// 课堂练习二 —— 硬币计数（OpenCV C++）
//
// 与课堂练习一（回形针）相比，本题目标是暗背景上的亮物体，因此不需要 THRESH_BINARY_INV；
// 同时硬币是近似圆形，可以额外用「圆度」把细长的边框线、噪点排掉。
//
// 处理流程：
//   1. 读图（二进制流 + imdecode，兼容中文路径）
//   2. 转灰度 + 高斯平滑
//   3. Otsu 二值化：硬币比背景亮，正常阈值即可让硬币成为白色前景
//   4. 形态学闭运算：填补硬币表面的高光/纹理造成的细小孔洞
//   5. 提取最外层轮廓（RETR_EXTERNAL）
//   6. 筛选：面积区间、圆度、不贴图像边界（排除照片边框）→ 计数
//   7. 标注编号并输出结果图

#include <opencv2/opencv.hpp>
// OpenCV 5.0 把 contourArea / boundingRect / findContours 等轮廓几何函数
// 从 imgproc 挪到了 geometry 模块，需要显式包含
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
    std::string input = "images/coins.jpg";
    std::string outDir = "results";
    int borderMargin = 8;          // 外接框离图像边界不足该像素数，视为照片边框
    double minAreaRatio = 0.003;   // 面积下限（相对整幅图像）
    double maxAreaRatio = 0.120;   // 面积上限
    double minCircularity = 0.55;  // 圆度下限：4πA/P²，正圆为 1
    double maxAspect = 2.5;        // 长宽比上限，硬币接近 1
    bool headless = false;
    bool debug = false;
};

void printUsage() {
    std::cout <<
        "用法: coin_count [图片路径] [选项]\n"
        "  默认图片: images/coins.jpg\n"
        "选项:\n"
        "  --out <目录>        结果输出目录（默认 results）\n"
        "  --headless          算完直接退出，不显示窗口\n"
        "  --debug             额外保存灰度图，并打印被丢弃的轮廓\n"
        "  --min-area-ratio <r>  面积下限比例（默认 0.003）\n"
        "  --max-area-ratio <r>  面积上限比例（默认 0.12）\n"
        "  --min-circularity <c> 圆度下限（默认 0.55）\n"
        "  --max-aspect <a>      长宽比上限（默认 2.5）\n"
        "  --margin <px>         贴边判定像素（默认 8）\n";
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

// 圆度：4π·面积 / 周长²，正圆 = 1，越细长越小
double circularity(const std::vector<cv::Point>& contour, double area) {
    double perimeter = cv::arcLength(contour, true);
    if (perimeter <= 0.0) return 0.0;
    return 4.0 * CV_PI * area / (perimeter * perimeter);
}

}  // namespace

int main(int argc, char** argv) {
    Options opt;
    std::string inputGiven;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](double& dst) {
            if (i + 1 < argc) dst = std::stod(argv[++i]);
        };
        if (a == "--help" || a == "-h") { printUsage(); return 0; }
        else if (a == "--out" && i + 1 < argc) opt.outDir = argv[++i];
        else if (a == "--headless") opt.headless = true;
        else if (a == "--debug") opt.debug = true;
        else if (a == "--min-area-ratio") next(opt.minAreaRatio);
        else if (a == "--max-area-ratio") next(opt.maxAreaRatio);
        else if (a == "--min-circularity") next(opt.minCircularity);
        else if (a == "--max-aspect") next(opt.maxAspect);
        else if (a == "--margin") { if (i + 1 < argc) opt.borderMargin = std::stoi(argv[++i]); }
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

    // 1) 灰度 + 平滑
    cv::Mat gray, blurred;
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 0);

    // 2) Otsu 正常阈值：硬币（亮）-> 255 前景
    cv::Mat binary;
    double otsuValue = cv::threshold(blurred, binary, 0, 255,
                                     cv::THRESH_BINARY | cv::THRESH_OTSU);
    std::cout << "Otsu 阈值: " << std::fixed << std::setprecision(1) << otsuValue << "\n";

    // 3) 闭运算填补表面高光/纹理造成的细小孔洞
    cv::Mat cleaned;
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(7, 7));
    cv::morphologyEx(binary, cleaned, cv::MORPH_CLOSE, kernel);

    // 4) 只要最外层轮廓
    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(cleaned, contours, hierarchy, cv::RETR_CCOMP, cv::CHAIN_APPROX_SIMPLE);

    double imgArea = static_cast<double>(src.cols) * src.rows;
    double minArea = opt.minAreaRatio * imgArea;
    double maxArea = opt.maxAreaRatio * imgArea;
    std::cout << "面积区间: " << static_cast<int>(minArea) << " ~ "
              << static_cast<int>(maxArea) << " 像素\n";
    std::cout << "候选轮廓总数: " << contours.size() << "\n\n";

    std::vector<int> kept;
    int rejectArea = 0, rejectShape = 0, rejectBorder = 0, rejectInner = 0;

    for (size_t i = 0; i < contours.size(); ++i) {
        if (hierarchy[i][3] != -1) { ++rejectInner; continue; }

        double area = cv::contourArea(contours[i]);
        cv::Rect box = cv::boundingRect(contours[i]);
        double circ = circularity(contours[i], area);
        double aspect = (box.height == 0 || box.width == 0)
                            ? 1e9
                            : static_cast<double>(std::max(box.width, box.height)) /
                                  std::min(box.width, box.height);
        bool touchesBorder =
            box.x <= opt.borderMargin ||
            box.y <= opt.borderMargin ||
            box.x + box.width >= src.cols - opt.borderMargin ||
            box.y + box.height >= src.rows - opt.borderMargin;

        std::string reason;
        if (area < minArea || area > maxArea) { reason = "面积越界"; ++rejectArea; }
        else if (circ < opt.minCircularity || aspect > opt.maxAspect) { reason = "形状不像圆"; ++rejectShape; }
        else if (touchesBorder) { reason = "贴图像边界"; ++rejectBorder; }

        if (reason.empty()) {
            kept.push_back(static_cast<int>(i));
            std::cout << std::left << std::setw(4) << ("#" + std::to_string(kept.size()))
                      << " 面积 " << std::right << std::setw(6) << static_cast<int>(area)
                      << "  圆度 " << std::fixed << std::setprecision(2) << std::setw(4) << circ
                      << "  外接框 " << std::setw(3) << box.width << "x" << std::setw(3) << box.height
                      << "  长宽比 " << std::setw(4) << aspect
                      << "  中心(" << box.x + box.width / 2 << "," << box.y + box.height / 2 << ")\n";
        } else if (opt.debug) {
            std::cout << "    丢弃 面积=" << static_cast<int>(area)
                      << " 圆度=" << circ << " 框=" << box.width << "x" << box.height
                      << " -> " << reason << "\n";
        }
    }

    std::cout << "\n=== 计数结果: " << kept.size() << " 枚硬币 ===\n";
    std::cout << "丢弃统计: 面积越界 " << rejectArea
              << "，形状不像圆 " << rejectShape
              << "，贴边 " << rejectBorder
              << "，内层轮廓 " << rejectInner << "\n";

    // 5) 标注输出
    cv::Mat canvas = src.clone();
    for (size_t k = 0; k < kept.size(); ++k) {
        int idx = kept[k];
        cv::drawContours(canvas, contours, idx, cv::Scalar(0, 220, 0), 2);
        cv::Rect box = cv::boundingRect(contours[idx]);
        cv::circle(canvas, cv::Point(box.x + box.width / 2, box.y + box.height / 2),
                   std::max(box.width, box.height) / 2, cv::Scalar(255, 160, 0), 1);
        cv::putText(canvas, std::to_string(k + 1), cv::Point(box.x, std::max(box.y - 5, 16)),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 0, 255), 2, cv::LINE_AA);
    }
    cv::putText(canvas, "count = " + std::to_string(kept.size()), cv::Point(14, src.rows - 16),
                cv::FONT_HERSHEY_SIMPLEX, 0.9, cv::Scalar(0, 0, 255), 2, cv::LINE_AA);

    fs::create_directories(fs::path(opt.outDir));
    std::string outCounted = (fs::path(opt.outDir) / "coins_counted.png").string();
    bool ok1 = writeImage(outCounted, canvas);
    bool ok2 = writeImage((fs::path(opt.outDir) / "coins_binary.png").string(), cleaned);
    std::cout << (ok1 ? "[保存] " : "[失败] ") << outCounted << "\n";
    std::cout << (ok2 ? "[保存] " : "[失败] ") << (fs::path(opt.outDir) / "coins_binary.png").string() << "\n";
    if (opt.debug) {
        writeImage((fs::path(opt.outDir) / "coins_gray.png").string(), gray);
    }

    if (!opt.headless) {
        cv::imshow("coins - counted", canvas);
        cv::imshow("binary", cleaned);
        std::cout << "\n按任意键退出（ESC 也可）...\n";
        cv::waitKey(0);
    }
    return 0;
}
