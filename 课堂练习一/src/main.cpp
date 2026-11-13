// 课堂练习一 —— 回形针计数（OpenCV C++）
//
// 处理流程：
//   1. 读图（走二进制流，兼容中文路径）
//   2. 转灰度 + 高斯平滑，压掉纸面纹理
//   3. Otsu 反向二值化：回形针钢丝比背景暗，取反向阈值让钢丝成为白色前景
//   4. 形态学闭运算：修补钢丝二值化后的细小断裂
//   5. 提取轮廓（RETR_CCOMP，便于统计每个轮廓内部有几个孔）
//   6. 按四条规则筛选：面积区间、长宽比、不贴图像边界（排除照片边框线）、含封闭内孔
//   7. 计数并在原图上标注编号，输出结果图

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
    std::string input = "images/paperclips.jpg";
    std::string outDir = "results";
    int borderMargin = 6;            // 外接矩形离图像边界不足该像素数，视为照片边框线
    double minAreaRatio = 0.0015;    // 面积下限（相对整幅图像像素数）
    double maxAreaRatio = 0.0300;    // 面积上限
    double maxAspect = 5.0;          // 长宽比上限：回形针是长条，但不会是细长直线
    bool requireHole = true;         // 回形针钢丝会围出封闭内孔
    bool headless = false;           // 无窗口模式：只算、只存图，不等按键
    bool debug = false;              // 额外输出中间图
};

void printUsage() {
    std::cout <<
        "用法: paperclip_count [图片路径] [选项]\n"
        "  默认图片: images/paperclips.jpg\n"
        "选项:\n"
        "  --out <目录>        结果输出目录（默认 results）\n"
        "  --headless          算完直接退出，不显示窗口、不等按键\n"
        "  --debug             额外保存灰度图与二值图\n"
        "  --min-area-ratio <r>  面积下限比例（默认 0.0015）\n"
        "  --max-area-ratio <r>  面积上限比例（默认 0.03）\n"
        "  --max-aspect <a>      长宽比上限（默认 5.0）\n"
        "  --margin <px>         贴边判定像素（默认 6）\n"
        "  --no-hole-check     关掉「必须有封闭内孔」这一条\n";
}

// 读取图片：用二进制流 + imdecode，中文路径和中文文件名都能正常处理
cv::Mat readImage(const std::string& path, int flags = cv::IMREAD_COLOR) {
    std::ifstream in(fs::path(path), std::ios::binary);
    if (!in) return {};
    std::vector<uchar> buf((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (buf.empty()) return {};
    return cv::imdecode(buf, flags);
}

// 保存图片：同样走 imencode，输出目录含中文也不出错
bool writeImage(const std::string& path, const cv::Mat& img) {
    std::vector<uchar> buf;
    std::vector<int> params{cv::IMWRITE_PNG_COMPRESSION, 3};
    if (!cv::imencode(".png", img, buf, params)) return false;
    std::ofstream out(fs::path(path), std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    return out.good();
}

// 统计某个外轮廓内部的孔洞个数
int countHoles(const std::vector<std::vector<cv::Point>>& contours,
               const std::vector<cv::Vec4i>& hierarchy, int idx) {
    int holes = 0;
    for (size_t j = 0; j < contours.size(); ++j) {
        if (hierarchy[j][3] == idx) ++holes;
    }
    return holes;
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
        else if (a == "--no-hole-check") opt.requireHole = false;
        else if (a == "--min-area-ratio") next(opt.minAreaRatio);
        else if (a == "--max-area-ratio") next(opt.maxAreaRatio);
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

    // 2) Otsu 反向二值化：钢丝（暗）-> 255 前景
    cv::Mat binary;
    double otsuValue = cv::threshold(blurred, binary, 0, 255,
                                     cv::THRESH_BINARY_INV | cv::THRESH_OTSU);
    std::cout << "Otsu 阈值: " << std::fixed << std::setprecision(1) << otsuValue << "\n";

    // 3) 闭运算修补钢丝断点（核 5x5，迭代 1 次）
    cv::Mat cleaned;
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::morphologyEx(binary, cleaned, cv::MORPH_CLOSE, kernel);

    // 4) 轮廓提取
    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(cleaned, contours, hierarchy, cv::RETR_CCOMP, cv::CHAIN_APPROX_SIMPLE);

    double imgArea = static_cast<double>(src.cols) * src.rows;
    double minArea = opt.minAreaRatio * imgArea;
    double maxArea = opt.maxAreaRatio * imgArea;
    std::cout << "面积区间: " << static_cast<int>(minArea) << " ~ "
              << static_cast<int>(maxArea) << " 像素\n";
    std::cout << "候选外轮廓总数: " << contours.size() << "\n\n";

    // 5) 逐轮廓判定
    std::vector<int> kept;                 // 通过筛选的轮廓下标
    int rejectArea = 0, rejectAspect = 0, rejectBorder = 0, rejectNoHole = 0, rejectInner = 0;

    for (size_t i = 0; i < contours.size(); ++i) {
        if (hierarchy[i][3] != -1) { ++rejectInner; continue; }   // 只要最外层轮廓

        double area = cv::contourArea(contours[i]);
        cv::Rect box = cv::boundingRect(contours[i]);
        double aspect = (box.height == 0 || box.width == 0)
                            ? 1e9
                            : static_cast<double>(std::max(box.width, box.height)) /
                                  std::min(box.width, box.height);

        bool touchesBorder =
            box.x <= opt.borderMargin ||
            box.y <= opt.borderMargin ||
            box.x + box.width >= src.cols - opt.borderMargin ||
            box.y + box.height >= src.rows - opt.borderMargin;

        int holes = countHoles(contours, hierarchy, static_cast<int>(i));

        std::string reason;
        if (area < minArea || area > maxArea) { reason = "面积越界"; ++rejectArea; }
        else if (aspect > opt.maxAspect)       { reason = "长宽比过大(直线/边框)"; ++rejectAspect; }
        else if (touchesBorder)                { reason = "贴图像边界"; ++rejectBorder; }
        else if (opt.requireHole && holes < 1) { reason = "无封闭内孔"; ++rejectNoHole; }

        if (reason.empty()) {
            kept.push_back(static_cast<int>(i));
            std::cout << std::left << std::setw(4) << ("#" + std::to_string(kept.size()))
                      << " 面积 " << std::right << std::setw(6) << static_cast<int>(area)
                      << "  外接框 " << std::setw(3) << box.width << "x" << std::setw(3) << box.height
                      << "  长宽比 " << std::fixed << std::setprecision(2) << std::setw(5) << aspect
                      << "  孔洞 " << holes
                      << "  中心(" << box.x + box.width / 2 << "," << box.y + box.height / 2 << ")\n";
        } else if (opt.debug) {
            std::cout << "    丢弃 面积=" << static_cast<int>(area)
                      << " 框=" << box.width << "x" << box.height
                      << " 长宽比=" << aspect
                      << " -> " << reason << "\n";
        }
    }

    std::cout << "\n=== 计数结果: " << kept.size() << " 个回形针 ===\n";
    std::cout << "丢弃统计: 面积越界 " << rejectArea
              << "，长宽比过大 " << rejectAspect
              << "，贴边 " << rejectBorder
              << "，无内孔 " << rejectNoHole
              << "，内层轮廓 " << rejectInner << "\n";

    // 6) 标注输出
    cv::Mat canvas = src.clone();
    for (size_t k = 0; k < kept.size(); ++k) {
        int idx = kept[k];
        cv::drawContours(canvas, contours, idx, cv::Scalar(0, 200, 0), 2);
        cv::Rect box = cv::boundingRect(contours[idx]);
        cv::rectangle(canvas, box, cv::Scalar(255, 160, 0), 1);
        cv::putText(canvas, std::to_string(k + 1), cv::Point(box.x, std::max(box.y - 4, 14)),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 230), 2, cv::LINE_AA);
    }
    cv::putText(canvas, "count = " + std::to_string(kept.size()), cv::Point(14, src.rows - 18),
                cv::FONT_HERSHEY_SIMPLEX, 0.9, cv::Scalar(0, 0, 230), 2, cv::LINE_AA);

    fs::create_directories(fs::path(opt.outDir));
    std::string outCounted = (fs::path(opt.outDir) / "paperclips_counted.png").string();
    bool ok1 = writeImage(outCounted, canvas);
    bool ok2 = writeImage((fs::path(opt.outDir) / "paperclips_binary.png").string(), cleaned);
    std::cout << (ok1 ? "[保存] " : "[失败] ") << outCounted << "\n";
    std::cout << (ok2 ? "[保存] " : "[失败] ") << (fs::path(opt.outDir) / "paperclips_binary.png").string() << "\n";
    if (opt.debug) {
        writeImage((fs::path(opt.outDir) / "paperclips_gray.png").string(), gray);
    }

    if (!opt.headless) {
        cv::imshow("paper clips - counted", canvas);
        cv::imshow("binary", cleaned);
        std::cout << "\n按任意键退出（ESC 也可）...\n";
        cv::waitKey(0);
    }
    return 0;
}
