//
// Created by LEI XU on 4/27/19.
//

#ifndef RASTERIZER_TEXTURE_H
#define RASTERIZER_TEXTURE_H
#include "global.hpp"
#include <algorithm>
#include <eigen3/Eigen/Eigen>
#include <opencv2/opencv.hpp>
class Texture {
  private:
    cv::Mat image_data; // 一个 OpenCV
                        // 的矩阵类型的变量，用来在内存里真正存放整张图片的数据。

  public:
    Texture(const std::string &name) {
        image_data = cv::imread(name); // 1. 调用 OpenCV 库，从你的硬盘路径 (name)
                                       // 读取图片，塞进巨大的 image_data 内存矩阵里
        cv::cvtColor(image_data, image_data,
                     cv::COLOR_RGB2BGR); // 2. 转换一下颜色格式（OpenCV 默认
                                         // BGR，OpenGL 习惯 RGB）
        width = image_data.cols;         // 3. 记录下图片的高和宽，方便后面做除法
        height =
            image_data.rows; // 注意 这里的col 可不是 color 而是 columns 行的意思
                             // 以及row 是 列的意思
    }

    int width, height;

    Eigen::Vector3f
    getColor(float u, float v) { // 之前作业2的获取颜色只会获取第一个顶点的颜色
        // 现在变成从uv坐标上提取了
        // BUGFIX 记录：
        // 上一版直接写：
        //   auto u_img = u * width;
        //   auto v_img = (1 - v) * height;
        // 然后用 image_data.at<cv::Vec3b>(v_img, u_img) 取像素。
        // 这里有两个问题：
        //   1. 透视矫正插值后的 uv 可能因为浮点误差略小于 0 或略大于 1；
        //   2. 即使 uv 正好等于边界，例如 u == 1，u * width 也会得到 width，
        //      但合法列号最大只能是 width - 1。
        // 所以上一版在 texture shader 下会出现 OpenCV 越界断言，严重时直接崩溃。
        //
        // 这一版先把 uv clamp 到 [0, 1]，再乘以 (width - 1)/(height - 1)，
        // 最后把整数像素坐标也 clamp 一次。这样边界 uv 会落在最后一个合法像素上，
        // 不会访问图片外面的内存。
        u = std::clamp(u, 0.0f, 1.0f);
        v = std::clamp(v, 0.0f, 1.0f);
        int u_img = std::clamp(static_cast<int>(u * (width - 1)), 0, width - 1);
        // 4. 核心公式：把 0~1 的纹理坐标 (u, v)
        // 转换成像素坐标 (u_img, v_img)
        // 注意：v 轴是反的（图片原点在左上角，屏幕原点在左下角），所以用 (1-v)
        int v_img =
            std::clamp(static_cast<int>((1.0f - v) * (height - 1)), 0, height - 1);
        // 很简单的逻辑，就是设定uv，然后把它和 image 的像素一一对应，这样 image
        // 就被映射到 uv 坐标了，就称为贴图了
        auto color = image_data.at<cv::Vec3b>(v_img, u_img); // 把color 赋值为 image 里面第v行 第u列的一个三维向量（rgb三个通道）
        // 因为image里面可以存各种各样的东西 所以你得告诉程序你要拿去什么
        // 至于说 at 他就是那个抓取的函数
        return Eigen::Vector3f(color[0], color[1], color[2]);
        // 返回对应的颜色
    }
};
#endif // RASTERIZER_TEXTURE_H
