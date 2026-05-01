//
// Created by goksu on 4/6/19.
//

#pragma once

#include "Shader.hpp"
#include "Triangle.hpp"
#include "global.hpp"
#include <algorithm>
#include <eigen3/Eigen/Eigen>
#include <optional>

using namespace Eigen;

namespace rst {
enum class Buffers { Color = 1,
                     Depth = 2 };

inline Buffers operator|(Buffers a, Buffers b) {
    return Buffers((int)a | (int)b);
}

inline Buffers operator&(Buffers a, Buffers b) {
    return Buffers((int)a & (int)b);
}

enum class Primitive { Line,
                       Triangle };

/*
 * For the curious : The draw function takes two buffer id's as its arguments.
 * These two structs make sure that if you mix up with their orders, the
 * compiler won't compile it. Aka : Type safety
 * */
struct pos_buf_id {
    int pos_id = 0;
};

struct ind_buf_id {
    int ind_id = 0;
};

struct col_buf_id {
    int col_id = 0;
};

class rasterizer {
  public:
    rasterizer(int w, int h);
    pos_buf_id load_positions(const std::vector<Eigen::Vector3f> &positions);
    ind_buf_id load_indices(const std::vector<Eigen::Vector3i> &indices);
    col_buf_id load_colors(const std::vector<Eigen::Vector3f> &colors);
    col_buf_id load_normals(const std::vector<Eigen::Vector3f> &normals);

    void set_model(const Eigen::Matrix4f &m);
    void set_view(const Eigen::Matrix4f &v);
    void set_projection(const Eigen::Matrix4f &p);

    void set_texture(Texture tex) { texture = tex; }

    void set_vertex_shader(
        std::function<Eigen::Vector3f(vertex_shader_payload)> vert_shader);
    void set_fragment_shader(
        std::function<Eigen::Vector3f(fragment_shader_payload)> frag_shader);

    void set_pixel(const Vector2i &point, const Eigen::Vector3f &color); // 妈的这里不能直接用原来的代码
    // 他改签名了，之前的都是Eigen::Vector3f，这次改成vector2i了
    // 因为作业2里要传入三维函数，然后在设置片元缓冲区内部做深度缓冲
    // 现在改了有专门的depth——buf了
    // 一句话：这次把颜色写入和深度处理职责分开了，所以 set_pixel 只要 2D 整数坐标。

    void clear(Buffers buff);

    void draw(pos_buf_id pos_buffer, ind_buf_id ind_buffer, col_buf_id col_buffer,
              Primitive type);
    void draw(std::vector<Triangle *> &TriangleList);

    std::vector<Eigen::Vector3f> &frame_buffer() { return frame_buf; }

  private:
    void draw_line(Eigen::Vector3f begin, Eigen::Vector3f end);

    void rasterize_triangle(const Triangle &t,
                            const std::array<Eigen::Vector3f, 3> &world_pos);

    // VERTEX SHADER -> MVP -> Clipping -> /.W -> VIEWPORT -> DRAWLINE/DRAWTRI ->
    // FRAGSHADER
    //

  private:
    Eigen::Matrix4f model;
    Eigen::Matrix4f view;
    Eigen::Matrix4f projection;

    int normal_id = -1;

    std::map<int, std::vector<Eigen::Vector3f>> pos_buf;
    std::map<int, std::vector<Eigen::Vector3i>> ind_buf;
    std::map<int, std::vector<Eigen::Vector3f>> col_buf;
    std::map<int, std::vector<Eigen::Vector3f>> nor_buf;

    std::optional<Texture> texture;

    std::function<Eigen::Vector3f(fragment_shader_payload)>
        fragment_shader; // 这里有个应用shader的部分，后续在光栅化cpp文件里就是通过这个调用的shader
    // 这是一个 std::function，可以存储任何签名为
    // Vector3f(fragment_shader_payload) 的函数
    std::function<Eigen::Vector3f(vertex_shader_payload)> vertex_shader;

    std::vector<Eigen::Vector3f> frame_buf;
    std::vector<float> depth_buf; // 诺 这里就是专门的深度缓冲期，所以说在set_pixel的部分就不用传入三维坐标了
    int get_index(int x, int y);

    int width, height;

    int next_id = 0;
    int get_next_id() { return next_id++; }
};
} // namespace rst
