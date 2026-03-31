// clang-format off
//
// Created by goksu on 4/6/19.
//

//变化就是多了个计算重心坐标还有颜色缓冲区
//（因为要花俩三角形，所以要引入不同地颜色缓冲区）
//然后让你插值算颜色和深度缓冲，还有判断内外，还有线性插值判断深度


#include <algorithm>
#include <vector>
#include "rasterizer.hpp"
#include <opencv2/opencv.hpp>
#include <math.h>


rst::pos_buf_id rst::rasterizer::load_positions(const std::vector<Eigen::Vector3f> &positions)
{
    auto id = get_next_id();
    pos_buf.emplace(id, positions);

    return {id};
}

rst::ind_buf_id rst::rasterizer::load_indices(const std::vector<Eigen::Vector3i> &indices)
{
    auto id = get_next_id();
    ind_buf.emplace(id, indices);

    return {id};
}

rst::col_buf_id rst::rasterizer::load_colors(const std::vector<Eigen::Vector3f> &cols)
{
    auto id = get_next_id();
    col_buf.emplace(id, cols);

    return {id};
}

auto to_vec4(const Eigen::Vector3f& v3, float w = 1.0f)
{
    return Vector4f(v3.x(), v3.y(), v3.z(), w);
}


static bool insideTriangle(int x, int y, const Vector3f* _v)//判断像素是否在三角形内
{   
    // 1. 取像素的中心点作为测试坐标
    float px = x + 0.5f;
    float py = y + 0.5f;

    // 2. 提取三个顶点的坐标
    float x0 = _v[0].x(), y0 = _v[0].y();
    float x1 = _v[1].x(), y1 = _v[1].y();
    float x2 = _v[2].x(), y2 = _v[2].y();

    // 3. 计算三条边的二维方向向量 (AB, BC, CA)
    float dx0 = x1 - x0; float dy0 = y1 - y0;
    float dx1 = x2 - x1; float dy1 = y2 - y1;
    float dx2 = x0 - x2; float dy2 = y0 - y2;

    // 4. 计算顶点指向测试像素点的二维向量 (AP, BP, CP)
    float dpx0 = px - x0; float dpy0 = py - y0;
    float dpx1 = px - x1; float dpy1 = py - y1;
    float dpx2 = px - x2; float dpy2 = py - y2;

    // 5. 分别计算二维叉乘的 Z 分量结果 (也就是 X1*Y2 - Y1*X2)
    float cross0 = dx0 * dpy0 - dy0 * dpx0;
    float cross1 = dx1 * dpy1 - dy1 * dpx1;
    float cross2 = dx2 * dpy2 - dy2 * dpx2;

    // 6. 只要这三个叉乘结果“同号”（比如要么全是正，要么全是负）
    // 就代表这个点被关在三条边的同一侧，一定是在三角形内部！
    // 巧妙利用了一个逻辑：如果既有正数又有负数，那就肯定是在外边了
    bool has_pos = (cross0 > 0) || (cross1 > 0) || (cross2 > 0);
    bool has_neg = (cross0 < 0) || (cross1 < 0) || (cross2 < 0);

    return !(has_pos && has_neg);
}

static std::tuple<float, float, float> computeBarycentric2D(float x, float y, const Vector3f* v)
{
    float c1 = (x*(v[1].y() - v[2].y()) + (v[2].x() - v[1].x())*y + v[1].x()*v[2].y() - v[2].x()*v[1].y()) / (v[0].x()*(v[1].y() - v[2].y()) + (v[2].x() - v[1].x())*v[0].y() + v[1].x()*v[2].y() - v[2].x()*v[1].y());
    float c2 = (x*(v[2].y() - v[0].y()) + (v[0].x() - v[2].x())*y + v[2].x()*v[0].y() - v[0].x()*v[2].y()) / (v[1].x()*(v[2].y() - v[0].y()) + (v[0].x() - v[2].x())*v[1].y() + v[2].x()*v[0].y() - v[0].x()*v[2].y());
    float c3 = (x*(v[0].y() - v[1].y()) + (v[1].x() - v[0].x())*y + v[0].x()*v[1].y() - v[1].x()*v[0].y()) / (v[2].x()*(v[0].y() - v[1].y()) + (v[1].x() - v[0].x())*v[2].y() + v[0].x()*v[1].y() - v[1].x()*v[0].y());
    return {c1,c2,c3};
}//计算重心坐标

void rst::rasterizer::draw(pos_buf_id pos_buffer, ind_buf_id ind_buffer, col_buf_id col_buffer, Primitive type)
{
    auto& buf = pos_buf[pos_buffer.pos_id];
    auto& ind = ind_buf[ind_buffer.ind_id];
    auto& col = col_buf[col_buffer.col_id];

    float f1 = (50 - 0.1) / 2.0;
    float f2 = (50 + 0.1) / 2.0;

    Eigen::Matrix4f mvp = projection * view * model;
    for (auto& i : ind)//这里就是便利所有的三角形索引
    {
        Triangle t;//分配在栈上 画完了就销毁
        Eigen::Vector4f v[] = {
                mvp * to_vec4(buf[i[0]], 1.0f),
                mvp * to_vec4(buf[i[1]], 1.0f),
                mvp * to_vec4(buf[i[2]], 1.0f)
        };
        //Homogeneous division
        for (auto& vec : v) {
            vec /= vec.w();
        }
        //Viewport transformation
        for (auto & vert : v)
        {
            vert.x() = 0.5*width*(vert.x()+1.0);
            vert.y() = 0.5*height*(vert.y()+1.0);
            vert.z() = vert.z() * f1 + f2;
        }

        for (int i = 0; i < 3; ++i)
        {
            t.setVertex(i, v[i].head<3>());
            t.setVertex(i, v[i].head<3>());
            t.setVertex(i, v[i].head<3>());
        }

        auto col_x = col[i[0]];
        auto col_y = col[i[1]];
        auto col_z = col[i[2]];

        t.setColor(0, col_x[0], col_x[1], col_x[2]);
        t.setColor(1, col_y[0], col_y[1], col_y[2]);
        t.setColor(2, col_z[0], col_z[1], col_z[2]);

        rasterize_triangle(t);
    }
}

//Screen space rasterization
void rst::rasterizer::rasterize_triangle(const Triangle& t) {
    auto v = t.toVector4();
    
    // TODO : Find out the bounding box of current triangle.

    // iterate through the pixel and find if the current pixel is inside the triangle
    // 1. 找出包围盒边界
    // 为了防止浮点误差少画像素，一般会通过强转 int 的方式处理，这里简单直接用 std::min/max
    int x_min = std::min(v[0].x(), std::min(v[1].x(), v[2].x()));
    int x_max = std::max(v[0].x(), std::max(v[1].x(), v[2].x()));
    int y_min = std::min(v[0].y(), std::min(v[1].y(), v[2].y()));
    int y_max = std::max(v[0].y(), std::max(v[1].y(), v[2].y()));

    // 2. 遍历包围盒里圈住的每一个像素方格 (注意循环条件应该是 <=，要不然边界可能画不全)
    for(int x = x_min; x <= x_max; ++x){
        for(int y = y_min; y <= y_max; ++y){
            
            // 3. 判断当前查房的这个像素点，到底在不在这个三角形内部？
            if(insideTriangle(x, y, t.v)){
                
                // 4. 如果在里面，那就用老师留下来的秘法：计算它的真实深度(插值Z)
                auto[alpha, beta, gamma] = computeBarycentric2D(x, y, t.v);
                float w_reciprocal = 1.0/(alpha / v[0].w() + beta / v[1].w() + gamma / v[2].w());
                float z_interpolated = alpha * v[0].z() / v[0].w() + beta * v[1].z() / v[1].w() + gamma * v[2].z() / v[2].w();
                z_interpolated *= w_reciprocal;

                // 5. 到账房去找这个像素格的历史账本记录，算出它的 1D 索引
                int idx = get_index(x, y);

                // 6. 深度测试 (Z-Buffer 核心灵魂！)
                // 把刚算出的真实深度 z_interpolated 去和账本 depth_buf[idx] 打一架
                // 因为我们一开始用的是无穷大初始化，所以谁的深度值更小，谁就离屏幕更近！
                if (z_interpolated < depth_buf[idx]) {
                    
                    // 打赢了第一步：把账本上的记录抹掉，改成自己的新深度（更小的宣誓主权）
                    depth_buf[idx] = z_interpolated;
                    
                    // 打赢了第二步：拿到当前这个三角形对象的颜色涂料，给屏幕上色！
                    set_pixel(Eigen::Vector3f(x, y, 1.0f), t.getColor());
                }
            }
        }
    }
}

void rst::rasterizer::set_model(const Eigen::Matrix4f& m)
{
    model = m;
}

void rst::rasterizer::set_view(const Eigen::Matrix4f& v)
{
    view = v;
}

void rst::rasterizer::set_projection(const Eigen::Matrix4f& p)
{
    projection = p;
}

void rst::rasterizer::clear(rst::Buffers buff)
{
    if ((buff & rst::Buffers::Color) == rst::Buffers::Color)
    {
        std::fill(frame_buf.begin(), frame_buf.end(), Eigen::Vector3f{0, 0, 0});
    }
    if ((buff & rst::Buffers::Depth) == rst::Buffers::Depth)
    {
        std::fill(depth_buf.begin(), depth_buf.end(), std::numeric_limits<float>::infinity());
    }
}

rst::rasterizer::rasterizer(int w, int h) : width(w), height(h)
{
    frame_buf.resize(w * h);
    depth_buf.resize(w * h);
}

int rst::rasterizer::get_index(int x, int y)
{
    return (height-1-y)*width + x;
}

void rst::rasterizer::set_pixel(const Eigen::Vector3f& point, const Eigen::Vector3f& color)
{
    //old index: auto ind = point.y() + point.x() * width;
    //改了这个的读取逻辑，变得和上面的清除帧缓存一致了
    auto ind = (height-1-point.y())*width + point.x();
    frame_buf[ind] = color;

}

// clang-format on