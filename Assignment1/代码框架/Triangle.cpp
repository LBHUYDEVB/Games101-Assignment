//
// Created by LEI XU on 4/11/19.
//

#include "Triangle.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>

Triangle::Triangle()//定义构造函数，初始化数组（要是只用默认构造函数就会出现一堆杂乱的内存）
{
    v[0] << 0, 0, 0;
    v[1] << 0, 0, 0;
    v[2] << 0, 0, 0;

    color[0] << 0.0, 0.0, 0.0;
    color[1] << 0.0, 0.0, 0.0;
    color[2] << 0.0, 0.0, 0.0;

    tex_coords[0] << 0.0, 0.0;
    tex_coords[1] << 0.0, 0.0;
    tex_coords[2] << 0.0, 0.0;
}

void Triangle::setVertex(int ind, Eigen::Vector3f ver) { v[ind] = ver; }//写入顶点信息

void Triangle::setNormal(int ind, Vector3f n) { normal[ind] = n; }//写入法线信息

void Triangle::setColor(int ind, float r, float g, float b)//写入颜色信息，并且规定颜色的范围
{
    if ((r < 0.0) || (r > 255.) || (g < 0.0) || (g > 255.) || (b < 0.0) ||
        (b > 255.)) {
        throw std::runtime_error("Invalid color values");
    }

    color[ind] = Vector3f((float)r / 255., (float)g / 255., (float)b / 255.);
    return;
}
void Triangle::setTexCoord(int ind, float s, float t)//写入uv坐标
{
    tex_coords[ind] = Vector2f(s, t);
}

std::array<Vector4f, 3> Triangle::toVector4() const
{
    std::array<Vector4f, 3> res;
    std::transform(std::begin(v), std::end(v), res.begin(), [](auto& vec) {
        return Vector4f(vec.x(), vec.y(), vec.z(), 1.f);
    });//气笑了 这里是个Lambda 表达式（匿名函数）他的意思就是直接在数组后面添加个1改成齐次坐标
//至于说提取出xyz这个操作是通过eigen的库里面实现的
//   那是怎么看出来vec是个vector3f类型的函数的呢？
//   编译器看到 std::begin(v)，它去查了一下 v。发现 v 是定义在  Triangle.hpp 里的 Vector3f v[3];。
// 所以，v 里面的每一个元素，都是实打实的 Eigen::Vector3f 类型。
// std::transform 的工作原理是从 v 里面挨个往外掏元素，掏出来第一个元素传给 Lambda 函数。
// 就在这个传递出去的瞬间，编译器发现传给  vec  的是一个 Eigen::Vector3f。
    return res;
}
//总结一下，triangle实现了写入顶点信息，法线信息，颜色信息，uv坐标，然后把顶点信息的矩阵转换为能表示平移的齐次坐标