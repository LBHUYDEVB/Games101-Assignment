//
// Created by goksu on 4/6/19.
//

#pragma once

#include "Triangle.hpp"
#include <algorithm>
#include <eigen3/Eigen/Eigen>
using namespace Eigen;

namespace rst {
enum class Buffers { Color = 1, Depth = 2 }; // 创建帧缓存类型

inline Buffers operator|(Buffers a, Buffers b) {
  return Buffers((int)a | (int)b);
} // 内链展开帧缓存的或运算，但是为啥？

inline Buffers operator&(Buffers a, Buffers b) {
  return Buffers((int)a & (int)b);
} // 同理 看不懂为啥

enum class Primitive { Line, Triangle }; // 定义了两种图元类型 线和三角
// 图元就是描述对象的几何要素的输出图元，称为几何图元，简称图元.
// 如点、直线段、圆、二次曲线、曲面等.

/*
 * For the curious : The draw function takes two buffer id's as its arguments.
 * These two structs make sure that if you mix up with their orders, the
 * compiler won't compile it. Aka : Type safety
 * */

//  “给好奇的读者：
// draw  函数接受两个缓冲区 ID（Buffer ID）作为参数。这两个结构体（ pos_buf_id和
// ind_buf_id）
// 确保了如果你不小心搞混了它们的传入顺序，编译器将无法通过编译。这就是所谓的：类型安全。”
// 假如说你直接用int作为传入类型 draw(int,int,Primitive)
// 你要是写反了根本看不出来，因为这俩是int值，传入那个都是合法的
// 所以说这就是一种防呆设计
struct pos_buf_id {
  int pos_id = 0;
};

struct ind_buf_id {
  int ind_id = 0;
};

class rasterizer {
public:
  rasterizer(int w, int h);
  pos_buf_id load_positions(
      const std::vector<Eigen::Vector3f> &positions); // 返回顶点位置的id
  ind_buf_id
  load_indices(const std::vector<Eigen::Vector3i> &indices); // 返回顶点索引的id

  void set_model(const Eigen::Matrix4f &m);      // 设置模型矩阵
  void set_view(const Eigen::Matrix4f &v);       // 设置视图矩阵
  void set_projection(const Eigen::Matrix4f &p, float zNear, float zFar); // 设置投影矩阵，同时传入近裁面和远裁面

  void set_pixel(const Eigen::Vector3f &point, const Eigen::Vector3f &color);//设置像素点

  void clear(Buffers buff);

  void draw(pos_buf_id pos_buffer, ind_buf_id ind_buffer, Primitive type);

  std::vector<Eigen::Vector3f> &frame_buffer() { return frame_buf; }

private:
  void draw_line(Eigen::Vector3f begin, Eigen::Vector3f end);
  void rasterize_wireframe(const Triangle &t);

private:
  Eigen::Matrix4f model;
  Eigen::Matrix4f view;
  Eigen::Matrix4f projection;//类里面存储三个矩阵

  std::map<int, std::vector<Eigen::Vector3f>>
      pos_buf; // 定义顶点的字典 上面是id 键值对
  std::map<int, std::vector<Eigen::Vector3i>> ind_buf; // 定义索引的字典

  std::vector<Eigen::Vector3f> frame_buf; // 帧缓存
  std::vector<float> depth_buf;           // 深度缓存
  int get_index(int x, int y);            // 获取像素索引

  int width, height;
  float zNear = 0.1f, zFar = 50.0f; // 近裁面和远裁面，默认值与 main.cpp 一致

  int next_id = 0;                        // 从0开始
  int get_next_id() { return next_id++; } // 草 直接写了个函数让id递增
};
} // namespace rst
