//
// Created by LEI XU on 4/27/19.
//

#ifndef RASTERIZER_SHADER_H
#define RASTERIZER_SHADER_H
#include "Texture.hpp"
#include <eigen3/Eigen/Eigen>

struct fragment_shader_payload // 这个头文件就是在加载着色器所需要的信息的
// 所以你看这里并没有什么详细的逻辑步骤，而是单纯的列出来变量
// 真正的shader实现逻辑是后续写的各个详细的着色器，着色器从这个payload中提取所需要的数据（法线
// 颜色 贴图之类的）
{
  fragment_shader_payload() {
    texture = nullptr;
    // 诶有人就要说了，为什么不在Texture类里写个构造函数，清理内存呢？
    // 因为这他妈是一个还没有实例化的类的指针，
    // 构造函数只能清理类的内存
    // 妈的我是傻逼
    // 这里只是在防止他指向杂乱的内存
    // 虽然咱们后续在光栅化步骤中传入的是全参，但是为了防止野指针，还是写上为好
    // 有人问为什么别的属性不用初始化指针，因为Eigen::Vector3f这个类内部有内存的优化
    //
  }

  fragment_shader_payload(const Eigen::Vector3f &col,
                          const Eigen::Vector3f &nor, const Eigen::Vector2f &tc,
                          Texture *tex)
      : color(col), normal(nor), tex_coords(tc), texture(tex) {
  } // 赋值 这是两种构造函数，假如说

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
