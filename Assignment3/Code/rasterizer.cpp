//
// Created by goksu on 4/6/19.
//

#include "rasterizer.hpp"
#include "Triangle.hpp"
#include <algorithm>
#include <math.h>
#include <opencv2/opencv.hpp>

rst::pos_buf_id
rst::rasterizer::load_positions(const std::vector<Eigen::Vector3f> &positions) {
    auto id = get_next_id();
    pos_buf.emplace(id, positions);

    return {id};
}

rst::ind_buf_id
rst::rasterizer::load_indices(const std::vector<Eigen::Vector3i> &indices) {
    auto id = get_next_id();
    ind_buf.emplace(
        id, indices); // 这里并没有调用这些，而是使用的别的trianglelist渲染流程

    return {id};
}

rst::col_buf_id
rst::rasterizer::load_colors(const std::vector<Eigen::Vector3f> &cols) {
    auto id = get_next_id();
    col_buf.emplace(id, cols);

    return {id};
}

rst::col_buf_id
rst::rasterizer::load_normals(const std::vector<Eigen::Vector3f> &normals) {
    auto id = get_next_id();
    nor_buf.emplace(id, normals);

    normal_id = id;

    return {id};
}

// Bresenham's line drawing algorithm
void rst::rasterizer::draw_line(Eigen::Vector3f begin, Eigen::Vector3f end) {
    auto x1 = begin.x();
    auto y1 = begin.y();
    auto x2 = end.x();
    auto y2 = end.y();

    Eigen::Vector3f line_color = {255, 255, 255};

    int x, y, dx, dy, dx1, dy1, px, py, xe, ye, i;

    dx = x2 - x1;
    dy = y2 - y1;
    dx1 = fabs(dx);
    dy1 = fabs(dy);
    px = 2 * dy1 - dx1;
    py = 2 * dx1 - dy1;

    if (dy1 <= dx1) {
        if (dx >= 0) {
            x = x1;
            y = y1;
            xe = x2;
        } else {
            x = x2;
            y = y2;
            xe = x1;
        }
        Eigen::Vector2i point = Eigen::Vector2i(x, y);
        set_pixel(point, line_color);
        for (i = 0; x < xe; i++) {
            x = x + 1;
            if (px < 0) {
                px = px + 2 * dy1;
            } else {
                if ((dx < 0 && dy < 0) || (dx > 0 && dy > 0)) {
                    y = y + 1;
                } else {
                    y = y - 1;
                }
                px = px + 2 * (dy1 - dx1);
            }
            //            delay(0);
            Eigen::Vector2i point = Eigen::Vector2i(x, y);
            set_pixel(point, line_color);
        }
    } else {
        if (dy >= 0) {
            x = x1;
            y = y1;
            ye = y2;
        } else {
            x = x2;
            y = y2;
            ye = y1;
        }
        Eigen::Vector2i point = Eigen::Vector2i(x, y);
        set_pixel(point, line_color);
        for (i = 0; y < ye; i++) {
            y = y + 1;
            if (py <= 0) {
                py = py + 2 * dx1;
            } else {
                if ((dx < 0 && dy < 0) || (dx > 0 && dy > 0)) {
                    x = x + 1;
                } else {
                    x = x - 1;
                }
                py = py + 2 * (dx1 - dy1);
            }
            //            delay(0);
            Eigen::Vector2i point = Eigen::Vector2i(x, y);
            set_pixel(point, line_color);
        }
    }
}

auto to_vec4(const Eigen::Vector3f &v3, float w = 1.0f) {
    return Vector4f(v3.x(), v3.y(), v3.z(), w);
}

static bool insideTriangle(float x, float y, const Vector4f *_v) {
    // BUGFIX 记录：
    // 上一版这里用的是齐次二维直线测试：
    //   f0 = v1.cross(v0), f1 = v2.cross(v1), f2 = v0.cross(v2)
    // 然后判断“采样点 p 和第三个顶点是否在同一侧”。这个思路本身可以做，
    // 但它对边界点和浮点误差很敏感；当采样点刚好落在三角形共享边附近时，
    // 两个相邻三角形可能都因为严格符号/误差问题拒绝这个像素。
    // 像素没有被任何三角形写入，就会保留 clear 后的背景色，也就是黑点/黑线。
    //
    // 这一版改成更直接的 edge function：
    //   edge(a,b,p) = cross(b-a, p-a)
    // 对三条边分别算符号。如果三者同为非负，或同为非正，就说明点在三角形内。
    // 同时保留一个很小的 eps，把接近边界的点也纳入，避免共享边漏像素。
    Eigen::Vector2f p(x, y);
    Eigen::Vector2f a(_v[0].x(), _v[0].y());
    Eigen::Vector2f b(_v[1].x(), _v[1].y());
    Eigen::Vector2f c(_v[2].x(), _v[2].y());

    auto edge = [](const Eigen::Vector2f &from, const Eigen::Vector2f &to,
                   const Eigen::Vector2f &point) {
        Eigen::Vector2f e = to - from;
        Eigen::Vector2f q = point - from;
        return e.x() * q.y() - e.y() * q.x();
    };

    constexpr float eps = 1e-5f;
    // 上一版没有显式处理退化三角形。若三个顶点几乎共线，后续重心坐标分母
    // 可能接近 0，结果会不稳定。这一版先用三角形面积的两倍做一次过滤。
    if (std::fabs(edge(a, b, c)) < eps)
        return false;

    float e0 = edge(a, b, p);
    float e1 = edge(b, c, p);
    float e2 = edge(c, a, p);

    return (e0 >= -eps && e1 >= -eps && e2 >= -eps) ||
           (e0 <= eps && e1 <= eps && e2 <= eps);
}

static std::tuple<float, float, float> computeBarycentric2D(float x, float y,
                                                            const Vector4f *v) {
    float c1 =
        (x * (v[1].y() - v[2].y()) + (v[2].x() - v[1].x()) * y +
         v[1].x() * v[2].y() - v[2].x() * v[1].y()) /
        (v[0].x() * (v[1].y() - v[2].y()) + (v[2].x() - v[1].x()) * v[0].y() +
         v[1].x() * v[2].y() - v[2].x() * v[1].y());
    float c2 =
        (x * (v[2].y() - v[0].y()) + (v[0].x() - v[2].x()) * y +
         v[2].x() * v[0].y() - v[0].x() * v[2].y()) /
        (v[1].x() * (v[2].y() - v[0].y()) + (v[0].x() - v[2].x()) * v[1].y() +
         v[2].x() * v[0].y() - v[0].x() * v[2].y());
    float c3 =
        (x * (v[0].y() - v[1].y()) + (v[1].x() - v[0].x()) * y +
         v[0].x() * v[1].y() - v[1].x() * v[0].y()) /
        (v[2].x() * (v[0].y() - v[1].y()) + (v[1].x() - v[0].x()) * v[2].y() +
         v[0].x() * v[1].y() - v[1].x() * v[0].y());
    return {c1, c2, c3};
}

void rst::rasterizer::draw(std::vector<Triangle *> &TriangleList) {

    float f1 = (50 - 0.1) / 2.0;
    float f2 = (50 + 0.1) / 2.0;

    Eigen::Matrix4f mvp = projection * view * model;
    for (const auto &t : TriangleList) {
        Triangle newtri = *t;

        std::array<Eigen::Vector4f, 3> mm{(view * model * t->v[0]),
                                          (view * model * t->v[1]),
                                          (view * model * t->v[2])};

        std::array<Eigen::Vector3f, 3> viewspace_pos;

        std::transform(mm.begin(), mm.end(), viewspace_pos.begin(),
                       [](auto &v) { return v.template head<3>(); });

        Eigen::Vector4f v[] = {mvp * t->v[0], mvp * t->v[1], mvp * t->v[2]};
        // Homogeneous division
        for (auto &vec : v) {
            vec.x() /= vec.w();
            vec.y() /= vec.w();
            vec.z() /= vec.w();
        }

        Eigen::Matrix4f inv_trans = (view * model).inverse().transpose();
        Eigen::Vector4f n[] = {inv_trans * to_vec4(t->normal[0], 0.0f),
                               inv_trans * to_vec4(t->normal[1], 0.0f),
                               inv_trans * to_vec4(t->normal[2], 0.0f)};

        // Viewport transformation
        for (auto &vert : v) {
            vert.x() = 0.5 * width * (vert.x() + 1.0);
            vert.y() = 0.5 * height * (vert.y() + 1.0);
            vert.z() = vert.z() * f1 + f2;
        }

        for (int i = 0; i < 3; ++i) {
            // screen space coordinates
            newtri.setVertex(i, v[i]);
        }

        for (int i = 0; i < 3; ++i) {
            // view space normal
            newtri.setNormal(i, n[i].head<3>());
        }

        newtri.setColor(0, 148, 121.0, 92.0);
        newtri.setColor(1, 148, 121.0, 92.0);
        newtri.setColor(2, 148, 121.0, 92.0);

        // Also pass view space vertice position
        rasterize_triangle(newtri, viewspace_pos);
    }
}

static Eigen::Vector3f interpolate(float alpha, float beta, float gamma,
                                   const Eigen::Vector3f &vert1,
                                   const Eigen::Vector3f &vert2,
                                   const Eigen::Vector3f &vert3, float weight) {
    return (alpha * vert1 + beta * vert2 + gamma * vert3) / weight;
} // 这里是对不同的属性进行插值，咱们这里应该传入的是深度矫正后的权重

static Eigen::Vector2f interpolate(float alpha, float beta, float gamma,
                                   const Eigen::Vector2f &vert1,
                                   const Eigen::Vector2f &vert2,
                                   const Eigen::Vector2f &vert3, float weight) {
    auto u = (alpha * vert1[0] + beta * vert2[0] + gamma * vert3[0]);
    auto v = (alpha * vert1[1] + beta * vert2[1] + gamma * vert3[1]);

    u /= weight;
    v /= weight;

    return Eigen::Vector2f(u, v);
} // 同理于上面，不过这里是插值uv坐标的

// Screen space rasterization
void rst::rasterizer::rasterize_triangle(
    const Triangle &t, const std::array<Eigen::Vector3f, 3> &view_pos) {
    auto v = t.v;
    float min_x = std::min(v[0].x(), std::min(v[1].x(), v[2].x()));
    float max_x = std::max(v[0].x(), std::max(v[1].x(), v[2].x()));
    float min_y = std::min(v[0].y(), std::min(v[1].y(), v[2].y()));
    float max_y = std::max(v[0].y(), std::max(v[1].y(), v[2].y()));

    // BUGFIX 记录：
    // 上一版直接把 float 类型的屏幕坐标赋给 int：
    //   int x_min = min_x;
    //   int x_max = max_x;
    // C++ 这样会直接截断小数部分。比如 max_x = 438.9 会变成 438，
    // 再配合 for (x < x_max)，实际最后只扫到 437。
    // 这会漏掉三角形右侧/上侧边界附近的候选像素；如果这些像素没有被相邻三角形补上，
    // frame buffer 里就还是清屏后的黑色，于是模型表面出现小黑框、小黑点或短黑线。
    //
    // 这一版的修法：
    //   min 用 floor，保证左/下边界不会少扫；
    //   max 用 ceil，保证右/上边界不会少扫；
    //   再 clamp 到 [0, width - 1] / [0, height - 1]，避免访问屏幕外。
    // 循环也从 x < x_max 改成 x <= x_max，因为 x_max 现在是合法的最后一个候选像素。
    int x_min = std::max(0, static_cast<int>(std::floor(min_x)));
    int x_max = std::min(width - 1, static_cast<int>(std::ceil(max_x)));
    int y_min = std::max(0, static_cast<int>(std::floor(min_y)));
    int y_max = std::min(height - 1, static_cast<int>(std::ceil(max_y)));

    for (int x = x_min; x <= x_max; x++) {
        for (int y = y_min; y <= y_max; y++) {
            // 像素中心是 (x + 0.5, y + 0.5)，不是整数角点 (x, y)。
            // 初版直接用整数点，相当于拿像素左下/左上角去做覆盖测试，
            // 更容易刚好压在三角形共享边上，从而触发 insideTriangle 的边界漏判。
            // 覆盖测试和重心坐标必须使用同一个采样点，否则颜色/深度会和覆盖不一致。
            float sample_x = x + 0.5f;
            float sample_y = y + 0.5f;
            if (insideTriangle(sample_x, sample_y, v)) {

                auto [alpha, beta, gamma] = computeBarycentric2D(sample_x, sample_y, v);
                float w_reciprocal =
                    1.0 / (alpha / v[0].w() + beta / v[1].w() + gamma / v[2].w());

                float z = alpha * v[0].z() / v[0].w() + beta * v[1].z() / v[1].w() +
                          gamma * v[2].z() / v[2].w();

                z = z * w_reciprocal; //  先计算出真实的z 值
                                      //  然后再通过争取的透视矫正原理计算出正确的权重，然后再对不同的属性进行插值
                float c_alpha = alpha / v[0].w() * w_reciprocal;
                float c_beta = beta / v[1].w() * w_reciprocal;
                float c_gamma = gamma / v[2].w() * w_reciprocal;
                auto interpolated_color = interpolate(
                    c_alpha, c_beta, c_gamma, t.color[0], t.color[1], t.color[2], 1.0);
                auto interpolated_normal =
                    interpolate(c_alpha, c_beta, c_gamma, t.normal[0], t.normal[1],
                                t.normal[2], 1.0);
                auto interpolated_texcoords =
                    interpolate(c_alpha, c_beta, c_gamma, t.tex_coords[0],
                                t.tex_coords[1], t.tex_coords[2], 1.0);
                auto interpolated_shadingcoords =
                    interpolate(c_alpha, c_beta, c_gamma, view_pos[0], view_pos[1],
                                view_pos[2], 1.0);
                int idx = get_index(x, y);

                // 6. 深度测试 (Z-Buffer 核心灵魂！)
                // 把刚算出的真实深度 z_interpolated 去和账本 depth_buf[idx] 打一架
                // 因为我们一开始用的是无穷大初始化，所以谁的深度值更小，谁就离屏幕更近！
                if (z < depth_buf[idx]) {

                    // 打赢了第一步：把账本上的记录抹掉，改成自己的新深度（更小的宣誓主权）
                    depth_buf[idx] = z;

                    fragment_shader_payload payload(                          // 这里就是在把着色器所需要的数据传入到一个位置，到时候应用到shader里
                        interpolated_color, interpolated_normal.normalized(), // 归一化
                        interpolated_texcoords,
                        texture
                            ? &*texture
                            : nullptr // 如果有贴图，就传贴图地址；没有贴图，就传空指针，对的
                                      // 这玩意和hpp里面设置贴图指针为空指针的内容重复了
                    );
                    payload.view_pos = interpolated_shadingcoords;
                    auto pixel_color = fragment_shader(payload); // 读取在main函数中存入的shader
                    set_pixel(Eigen::Vector2i(x, y), pixel_color);
                }
            }
        }
    }
    // TODO: From your HW3, get the triangle rasterization code.
    // TODO: Inside your rasterization loop:
    //    * v[i].w() is the vertex view space depth value z.
    //    * Z is interpolated view space depth for the current pixel
    //    * zp is depth between zNear and zFar, used for z-buffer

    // float Z = 1.0 / (alpha / v[0].w() + beta / v[1].w() + gamma / v[2].w());
    // float zp = alpha * v[0].z() / v[0].w() + beta * v[1].z() / v[1].w() + gamma
    // * v[2].z() / v[2].w(); zp *= Z;

    // TODO: Interpolate the attributes:
    // auto interpolated_color
    // auto interpolated_normal
    // auto interpolated_texcoords
    // auto interpolated_shadingcoords

    // Use: fragment_shader_payload payload( interpolated_color,
    // interpolated_normal.normalized(), interpolated_texcoords, texture ?
    // &*texture : nullptr); Use: payload.view_pos = interpolated_shadingcoords;
    // Use: Instead of passing the triangle's color directly to the frame buffer,
    // pass the color to the shaders first to get the final color; Use: auto
    // pixel_color = fragment_shader(payload);
}

void rst::rasterizer::set_model(const Eigen::Matrix4f &m) { model = m; }

void rst::rasterizer::set_view(const Eigen::Matrix4f &v) { view = v; }

void rst::rasterizer::set_projection(const Eigen::Matrix4f &p) {
    projection = p;
}

void rst::rasterizer::clear(rst::Buffers buff) {
    if ((buff & rst::Buffers::Color) == rst::Buffers::Color) {
        std::fill(frame_buf.begin(), frame_buf.end(), Eigen::Vector3f{0, 0, 0});
    }
    if ((buff & rst::Buffers::Depth) == rst::Buffers::Depth) {
        std::fill(depth_buf.begin(), depth_buf.end(),
                  std::numeric_limits<float>::infinity());
    }
}

rst::rasterizer::rasterizer(int w, int h) : width(w), height(h) {
    frame_buf.resize(w * h);
    depth_buf.resize(w * h);

    texture = std::nullopt;
}

int rst::rasterizer::get_index(int x, int y) {
    // 屏幕坐标 y=0 表示最下面一行，但 frame buffer 内存从第一行开始连续存。
    // 因此要翻转 y。初版使用 height - y，会让 y=0 映射到 height * width，
    // 正好越过最后一个合法元素；正确的最后一行索引应该是 height - 1。
    return (height - 1 - y) * width + x;
}

void rst::rasterizer::set_pixel(const Vector2i &point,
                                const Eigen::Vector3f &color) {
    // old index: auto ind = point.y() + point.x() * width;
    // 和 get_index 保持同一套坐标约定：把屏幕空间的 y 翻成缓冲区行号。
    int ind = (height - 1 - point.y()) * width + point.x();
    frame_buf[ind] = color;
}

void rst::rasterizer::set_vertex_shader(
    std::function<Eigen::Vector3f(vertex_shader_payload)> vert_shader) {
    vertex_shader = vert_shader;
}

void rst::rasterizer::set_fragment_shader( // 在这里载入shader
    std::function<Eigen::Vector3f(fragment_shader_payload)> frag_shader) {
    fragment_shader = frag_shader;
}
