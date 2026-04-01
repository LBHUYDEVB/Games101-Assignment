//
// Created by LEI XU on 4/27/19.
//

#ifndef RASTERIZER_TEXTURE_H
#define RASTERIZER_TEXTURE_H
#include "global.hpp"
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
    height = image_data.rows;
  }

  int width, height;

  Eigen::Vector3f getColor(float u, float v) {
    auto u_img = u * width; // 4. 核心公式：把 0~1 的纹理坐标 (u, v)
                            // 转换成像素坐标 (u_img, v_img)
    // 注意：v 轴是反的（图片原点在左上角，屏幕原点在左下角），所以用 (1-v)
    auto v_img = (1 - v) * height;
    auto color = image_data.at<cv::Vec3b>(v_img, u_img);
    return Eigen::Vector3f(color[0], color[1], color[2]);
  }
};
#endif // RASTERIZER_TEXTURE_H
