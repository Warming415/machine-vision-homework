/**
 * 机器视觉课程作业 —— 实验 01：RGB 图像像素操作
 *
 * 课件原始示例（逐像素把 R/G/B 三个通道各除以 2，图像整体变暗）：
 *
 *   Mat srcMat = imread("D:\\coins.png");
 *   int height = srcMat.rows;
 *   int width  = srcMat.cols;
 *   for (int j = 0; j < height; j++)
 *       for (int i = 0; i < width; i++) {
 *           srcMat.at<Vec3b>(j, i)[0] = srcMat.at<Vec3b>(j, i)[0] / 2;
 *           srcMat.at<Vec3b>(j, i)[1] = srcMat.at<Vec3b>(j, i)[1] / 2;
 *           srcMat.at<Vec3b>(j, i)[2] = srcMat.at<Vec3b>(j, i)[2] / 2;
 *       }
 *   imshow("test", srcMat);
 *   waitKey(0);
 *
 * 本程序在保留上述核心逻辑的前提下扩展了三点：
 *   1. 支持整目录批处理：默认读取 D:\chiikawa，目录不存在时读取工程内 images/；
 *   2. 支持中文路径：用 imdecode / imencode 绕开 imread / imwrite 的编码问题；
 *   3. 支持 --headless 无窗口模式，直接输出结果图，便于批处理与自动化验证。
 *
 * 用法：
 *   rgb_pixel_demo.exe                        # 处理默认目录 D:\chiikawa（不存在则用 images/）
 *   rgb_pixel_demo.exe <图片或文件夹>            # 处理指定图片或整个文件夹
 *   rgb_pixel_demo.exe <目标> --out results     # 指定输出目录（默认 output）
 *   rgb_pixel_demo.exe <目标> --headless        # 不弹窗口，只保存结果图
 */

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

#include <opencv2/opencv.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace cv;
using namespace std;

namespace {

const vector<string> kImageExt = {".png", ".jpg", ".jpeg", ".bmp",
                                 ".tif", ".tiff", ".webp"};

string toLower(string s) {
    transform(s.begin(), s.end(), s.begin(),
              [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
    return s;
}

bool isImageFile(const fs::path& p) {
    const string ext = toLower(p.extension().string());
    return find(kImageExt.begin(), kImageExt.end(), ext) != kImageExt.end();
}

/** 读取图片，路径可以是中文；失败返回空 Mat。 */
Mat imreadUnicode(const fs::path& path, int flags = IMREAD_COLOR) {
    ifstream in(path, ios::binary);
    if (!in) {
        return Mat();
    }
    vector<uchar> buf((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
    if (buf.empty()) {
        return Mat();
    }
    return imdecode(buf, flags);
}

/** 写出图片，路径可以是中文；格式由扩展名决定。 */
bool imwriteUnicode(const fs::path& path, const Mat& img) {
    string ext = path.extension().string();
    if (ext.empty()) {
        ext = ".png";
    }
    vector<uchar> buf;
    if (!imencode(ext, img, buf)) {
        return false;
    }
    ofstream out(path, ios::binary);
    if (!out) {
        return false;
    }
    out.write(reinterpret_cast<const char*>(buf.data()),
              static_cast<streamsize>(buf.size()));
    return out.good();
}

/**
 * 课件核心：逐像素、逐通道把像素值除以 2。
 * at<Vec3b>(行, 列) 返回 BGR 三通道像素，[0]=B、[1]=G、[2]=R。
 */
Mat darkenHalf(const Mat& src) {
    Mat dst = src.clone();
    const int height = dst.rows;
    const int width = dst.cols;
    for (int j = 0; j < height; ++j) {
        for (int i = 0; i < width; ++i) {
            Vec3b& px = dst.at<Vec3b>(j, i);
            px[0] = static_cast<uchar>(px[0] / 2);
            px[1] = static_cast<uchar>(px[1] / 2);
            px[2] = static_cast<uchar>(px[2] / 2);
        }
    }
    return dst;
}

vector<fs::path> collectInputs(const fs::path& target) {
    vector<fs::path> files;
    if (fs::is_directory(target)) {
        for (const auto& entry : fs::directory_iterator(target)) {
            if (entry.is_regular_file() && isImageFile(entry.path())) {
                files.push_back(entry.path());
            }
        }
        sort(files.begin(), files.end());
    } else if (fs::is_regular_file(target)) {
        files.push_back(target);
    }
    return files;
}

/** 默认输入目录：优先 D:\chiikawa，其次工程内 images/。 */
fs::path defaultInputDir() {
    const fs::path kPreferred = "D:/chiikawa";
    if (fs::exists(kPreferred) && fs::is_directory(kPreferred)) {
        return kPreferred;
    }
    return "images";
}

void printUsage() {
    cout << "用法:\n"
         << "  rgb_pixel_demo.exe [图片或文件夹] [--out 输出目录] [--headless]\n"
         << "\n"
         << "  不带参数时默认读取 D:\\chiikawa（不存在则读取工程内 images/）\n"
         << "  --out DIR    输出目录，默认 output\n"
         << "  --headless   不弹窗口，只保存结果图\n";
}

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    bool headless = false;
    fs::path outDir = "output";
    fs::path target;

    for (int k = 1; k < argc; ++k) {
        const string arg = argv[k];
        if (arg == "--headless") {
            headless = true;
        } else if (arg == "--out" && k + 1 < argc) {
            outDir = argv[++k];
        } else if (arg == "-h" || arg == "--help") {
            printUsage();
            return 0;
        } else {
            target = arg;
        }
    }

    if (target.empty()) {
        target = defaultInputDir();
    }

    cout << "输入目标: " << target.string() << "\n";

    if (!fs::exists(target)) {
        cout << "错误：路径不存在 —— " << target.string() << "\n"
             << "请把图片放进该目录，或在命令行中传入图片/文件夹路径。\n";
        return 1;
    }

    const vector<fs::path> files = collectInputs(target);
    if (files.empty()) {
        cout << "错误：目录中没有可处理的图片（支持 png/jpg/jpeg/bmp/tif/tiff/webp）。\n";
        return 1;
    }

    fs::create_directories(outDir);
    cout << "输出目录: " << outDir.string() << "\n"
         << "共 " << files.size() << " 张待处理\n";

    int handled = 0;
    for (const fs::path& file : files) {
        Mat src = imreadUnicode(file, IMREAD_COLOR);
        if (src.empty()) {
            cout << "[跳过] 读取失败: " << file.string() << "\n";
            continue;
        }

        Mat dark = darkenHalf(src);

        // 平均值应约为原图的一半，可直接作为「像素除以 2」是否生效的验证指标。
        const Scalar meanSrc = mean(src);
        const Scalar meanDark = mean(dark);

        cout << "[处理] " << file.filename().string() << "\n"
             << "        尺寸 " << src.cols << " x " << src.rows
             << "，通道 " << src.channels() << "\n"
             << "        平均 B/G/R: " << meanSrc[0] << " / " << meanSrc[1] << " / "
             << meanSrc[2] << "  ->  " << meanDark[0] << " / " << meanDark[1] << " / "
             << meanDark[2] << "\n";

        const fs::path darkPath = outDir / (file.stem().string() + "_dark.png");
        const fs::path cmpPath = outDir / (file.stem().string() + "_compare.png");

        if (!imwriteUnicode(darkPath, dark)) {
            cout << "[警告] 结果图写入失败: " << darkPath.string() << "\n";
        }
        cout << "        已保存: " << darkPath.string() << "\n";

        // 左右并排的对比图，方便一眼看出变暗效果。
        Mat cmp;
        if (src.cols > 1600) {
            Mat s1, s2;
            resize(src, s1, Size(), 0.5, 0.5, INTER_AREA);
            resize(dark, s2, Size(), 0.5, 0.5, INTER_AREA);
            hconcat(s1, s2, cmp);
        } else {
            hconcat(src, dark, cmp);
        }
        if (imwriteUnicode(cmpPath, cmp)) {
            cout << "        对比图: " << cmpPath.string() << "\n";
        }

        ++handled;

        if (!headless) {
            const string win = "RGB pixel op - " + file.filename().string();
            imshow(win, cmp);
            cout << "        按任意键继续，按 ESC 退出\n";
            const int key = waitKey(0);
            destroyWindow(win);
            if (key == 27) {
                break;
            }
        }
    }

    cout << "完成，共处理 " << handled << " 张。\n";
    return handled > 0 ? 0 : 1;
}
