//
// Created by LEI XU on 4/27/19.
//

#ifndef RASTERIZER_SHADER_H
#define RASTERIZER_SHADER_H
#include "Texture.hpp"
#include <eigen3/Eigen/Eigen>

struct fragment_shader_payload // 这个头文件就是在加载着色器所需要的信息的
// 所以你看这里并没有什么详细的逻辑步骤，而是单纯的列出来变量
{
  fragment_shader_payload() {
    texture = nullptr; // 初始化贴图
    // 至于说为什么只初始化这个
    // 因为其他的贴图都是eigen库里面定义好了的类
    // 他们有自己的初始化步骤
    // 但是texture只是个自己写的类的指针

    // 诶有人就要说了，为什么不在类里写个构造函数，清理内存呢？
    // 因为这他妈是一个类的指针
    // 类的内存还不知道呢，更何况构造函数只能清理类的内存
    // 妈的我是傻逼
    // 这里只是在防止他指向杂乱的内存
  }

  fragment_shader_payload(const Eigen::Vector3f &col,
                          const Eigen::Vector3f &nor, const Eigen::Vector2f &tc,
                          Texture *tex)
      : color(col), normal(nor), tex_coords(tc), texture(tex) {} // 赋值

  Eigen::Vector3f view_pos;
  Eigen::Vector3f color;
  Eigen::Vector3f normal;
  Eigen::Vector2f tex_coords;
  Texture *texture; // 诺 这里搞了个指针
};

struct vertex_shader_payload { // 加载顶点着色器所需要的东西
  Eigen::Vector3f position;
};

#endif // RASTERIZER_SHADER_H
