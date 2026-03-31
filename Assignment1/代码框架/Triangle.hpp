//
// Created by LEI XU on 4/11/19.
//

#ifndef RASTERIZER_TRIANGLE_H//这行是为了防止c++重估编译这个头文件的，因为你在编写的时候会重复include这个头文件
//但是在编译的时候理论上来讲只能出现一次（多次出现有重复声明）
// (if not defined) 如果在整个编译过程中，还没定义过这个名字
#define RASTERIZER_TRIANGLE_H// 那就立刻定义这个名字！

#include <eigen3/Eigen/Eigen>

using namespace Eigen;
class Triangle//这里有个小技巧，因为你实现头文件声明的cpp文件需要头文件中定变量
//但是头文件不能重复定义，为了防止这个问题，你可以专门创建一个类来存储变量
//
{
  public:
    Vector3f v[3]; /*the original coordinates of the triangle, v0, v1, v2 in
                      counter clockwise order*/
    /*Per vertex values*/
    Vector3f color[3];      // color at each vertex;
    Vector2f tex_coords[3]; // texture u,v
    Vector3f normal[3];     // normal vector for each vertex
    //存储uv 法线 顶点颜色

    // Texture *tex;
    Triangle();

    Eigen::Vector3f a() const { return v[0]; }
    Eigen::Vector3f b() const { return v[1]; }
    Eigen::Vector3f c() const { return v[2]; }//告诉你这是三角形三个顶点 出于易读性的目的写的
    //以后你在写算法时，写 triangle.a() 绝对比写 triangle.v[0] 看起来更直观、更不容易出错。

    void setVertex(int ind, Vector3f ver); /*set i-th vertex coordinates */
    void setNormal(int ind, Vector3f n);   /*set i-th vertex normal vector*/
    void setColor(int ind, float r, float g, float b); /*set i-th vertex color*/
    void setTexCoord(int ind, float s,
                     float t); /*set i-th vertex texture coordinate*/
    std::array<Vector4f, 3> toVector4() const;
};

#endif // RASTERIZER_TRIANGLE_H
