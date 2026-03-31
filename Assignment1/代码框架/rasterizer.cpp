//
// Created by goksu on 4/6/19.
//

#include "rasterizer.hpp"
#include <algorithm>
#include <math.h>
#include <opencv2/opencv.hpp>
#include <stdexcept>

rst::pos_buf_id rst::rasterizer::load_positions(
    const std::vector<Eigen::Vector3f> &positions) // 传入一个三维向量的引用
// 减少性能消耗，并且承诺不修改const 返回一个pos_buf_id
{
  auto id = get_next_id();        // 获得下一个要渲染的模型的位置id
  pos_buf.emplace(id, positions); // 去 pos_buf 字典里找个空位，直接用 id！！
                                  // 作为键、positions 作为值
  // 在那个空位上当场把这对数据给我建出来

  return {id};
}

rst::ind_buf_id rst::rasterizer::load_indices(
    const std::vector<Eigen::Vector3i> &indices) // 传入索引
{
  auto id = get_next_id();      // 获得下一个要渲染的模型的索引id！！
  ind_buf.emplace(id, indices); // 在对应位置创建索引的键值对

  return {id};
}

// Bresenham's line drawing algorithm
// Code taken from a stack overflow answer: https://stackoverflow.com/a/16405254
void rst::rasterizer::draw_line(Eigen::Vector3f begin,
                                Eigen::Vector3f end) // 划线 传入起始点
{

  auto x1 = begin.x();
  auto y1 = begin.y();
  auto x2 = end.x();
  auto y2 = end.y(); // 这里不考虑z值，是因为咱们之前所有的3D工作已经结束了
  // 世界空间已经完全转换为屏幕空间了

  Eigen::Vector3f line_color = {255, 255, 255}; // 选择线的颜色

  int x, y, dx, dy, dx1, dy1, px, py, xe, ye, i;

  dx = x2 - x1;
  dy = y2 - y1;
  dx1 = fabs(dx);
  dy1 = fabs(dy);     // fabs是求浮点数绝对值的，就是abs的绝对值版本
  px = 2 * dy1 - dx1; // 误差累加器 整个算法的核心，x每走一步就累加一次误差
  // 最后累积到一定地步再让y进一位
  py = 2 * dx1 - dy1;

  if (dy1 <= dx1) // 假如斜率小于1
  {
    if (dx >= 0) // 线段正向移动
    {
      x = x1;
      y = y1;
      xe = x2; // 划线的终点和传入的终点相同
    } else {
      x = x2;
      y = y2;
      xe = x1; // 如果线段往左，那就反过来，把终点当起点，依然从左往右画
    }
    Eigen::Vector3f point = Eigen::Vector3f(x, y, 1.0f); // 因为画像素点的函数
    // 只能传入三维向量，所以说这里加了个z值（因为后面要搞深度缓冲）
    set_pixel(point, line_color); // 先把初始点画出来
    for (i = 0; x < xe; i++)      // 做遍历了，开始划线
    {
      x = x + 1;  // 像素固定向右走
      if (px < 0) // 如果误差小于0，就直接向右划一个，不用向上画，y不变
      {
        px = px + 2 * dy1; // 只修改误差累加器
      } else               // 如果误差大于0，则要让y往上走一位
      {
        if ((dx < 0 && dy < 0) || (dx > 0 && dy > 0)) // 斜率大于0
        {
          y = y + 1; // 向上画
        } else {
          y = y - 1; // 向下画
        }
        px = px + 2 * (dy1 - dx1); // 修改误差累加器，详细推导见笔记
      }
      //            delay(0);
      Eigen::Vector3f point =
          Eigen::Vector3f(x, y, 1.0f); // 计算完y值的位置后，重新设定点
      set_pixel(point, line_color);    // 画下一个像素点
    }
  } else // 假如斜率大于1
  {
    if (dy >= 0) {
      x = x1;
      y = y1;
      ye = y2;
    } else // 这里很有趣，他相当于把坐标系倒过来，把y当作横轴了
    // 这么干是为了保证每个像素都能画出来
    // 你可以这么想，假如说斜率大于一的函数，横轴每变化1 ，纵轴不一定变化1啊
    // 他有可能变化多个，比方说斜率为2，你要是再以x为横轴，每次变化1
    // 那么y就会隔一个画一个，所以说为了保证这个问题不再出现
    // 他天才的才用了转换坐标轴的方法，永远让变化率最小的当纵轴
    {
      x = x2;
      y = y2;
      ye = y1;
    }
    Eigen::Vector3f point = Eigen::Vector3f(x, y, 1.0f);
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
      } // 这个时候有的傻逼就会问了（就是我），你在计算坐标的时候用了这么多奇技淫巧

      // 为什么画出来的图像还是正确的？？
      // 其实很简单，因为真正开始画画的函数是set_piexl
      // 他只需要计算出正确到哪个像素需要填色 至于说像素是这么算出来的，他不在乎
      // 只用把像素算对，算全就行了
      // 天才，真的是天才
      // delay(0);
      Eigen::Vector3f point = Eigen::Vector3f(x, y, 1.0f);
      set_pixel(point, line_color);
    }
  }
}

auto to_vec4(const Eigen::Vector3f &v3,
             float w = 1.0f) // 转换为齐次坐标，默认w为1
// 诶有人会说，triangle的函数里面不是写了个转换为四维向量的方法吗？
// 为什么这里还要写一个？
// 其实原因很简单，现在传入的只是空间中杂乱的点
// 咱们之后要干的就是把他按照索引包装成三角形
// 而triangle的toVector4f只有在包装成三角形才能用
// 这里是为了计算方便临时写的一个
{
  return Vector4f(v3.x(), v3.y(), v3.z(), w);
}

void rst::rasterizer::draw(rst::pos_buf_id pos_buffer,
                           rst::ind_buf_id ind_buffer, rst::Primitive type)
// 光栅化的核心函数，处理三维空间，和画线不同，这里处理的是三角形
{
  if (type != rst::Primitive::Triangle) {
    throw std::runtime_error(
        "Drawing primitives other than triangle is not implemented yet!");
  } // 只接受三角形图元
  auto &buf = pos_buf
      [pos_buffer
           .pos_id]; // 上面的load函数把坐标和索引存入词典，然后再用这个函数把他们取出来
  auto &ind = ind_buf[ind_buffer.ind_id]; // 获取顶点坐标和索引

  float f1 =
      (zFar - zNear) / 2.0; // 深度范围映射系数 f1: Scale（缩放系数 / 半宽）
  float f2 = (zFar + zNear) / 2.0; // f2: Bias（偏移量 / 中点）
  // 他俩的作用就是把z的范围从【-1，1】NDC空间
  // 拉伸到【n，f】屏幕空间（n是近裁面，f是远裁面）
  // 对的 屏幕空间是有深度值的，深度值只有最后做完深度缓冲的时候才就被抛弃
  // 方便后续的深度缓冲把对应的深度存储为屏幕空间的数值，而不是对应的在【-1，1】的数值，提升可读性
  // （其实还有，这个可以规避负数浮点数）
  // 早期的显卡不支持负数，所以说要规避负数
  // 注意 这种方法不能规避深度闪烁，他只是单纯的线性变换
  // 因为透视投影已经把深度精度分布定死了，两个原本因精度不足而无法区分的深度值，线性变换后依然无法区分
  // 所以说该出现深度闪烁还会出现
  // 源代码这里写的是，100-0.1是硬编码，为了解决这个问题我重新声明了俩变量，后面的人呢可以来看看💗
  Eigen::Matrix4f mvp = projection * view * model; // MVP矩阵的运算
  // 因为我们专门把光栅化定义成一个类 所以在运行的时候会
  // 专门实例化一遍rasterizer ，
  // 然后一步一步传入计算出来的矩阵，
  // 最后调用他内部的draw函数
  for (auto &i : ind) {
    Triangle t; // 要给他封装进三角形了

    Eigen::Vector4f v[] = {mvp * to_vec4(buf[i[0]], 1.0f),
                           mvp * to_vec4(buf[i[1]], 1.0f),
                           mvp * to_vec4(buf[i[2]], 1.0f)};
                           //草了这里要注意！！！他的索引和顶点的逻辑是这样的
                           // 先把这一大坨数组给他存到字典的一个位置里面
                           //然后在这里用索引的遍历一个一个给顶点从数组里拆出来

    // 把三维坐标变成齐次坐标，然后再乘以mvp矩阵

    for (auto &vec : v) { // 对于每个矩阵的的数，都除以他的w，确保w为1

      vec /= vec.w(); // 这里得会想起透视到正交变换
      // 因为矩阵乘法他只会改变系数，但是变换后的矩阵中的1/z就说明变换矩阵中有着点的相关数据
      // 又因为变换矩阵要同时应用在几百万个点上，咱们不可能算每个点的时候再把他的数据专门计算一个变换矩阵
      // 所以说要把一切有关于点本身的数据影响清除。
      // 于是就全部乘以z，把z存储到w里面，然后经过复杂的推导得到了透视到正交的矩阵。
      // 最后在把w归一化的时候相当于大伙都除以了一个自己的常量z，符合了透视的逻辑，是一种偶然间的必然
    }

    for (auto &vert :
         v) // 把投影后在NDC空间里的坐标翻译成屏幕坐标（视口变换矩阵）
    {
      vert.x() = 0.5 * width * (vert.x() + 1.0);
      vert.y() = 0.5 * height * (vert.y() + 1.0);
      vert.z() = vert.z() * f1 + f2; // z值也同样拉伸到【n，f】
    }

    for (int i = 0; i < 3; ++i) {
      t.setVertex(i, v[i].head<3>());
      t.setVertex(i, v[i].head<3>());
      t.setVertex(i, v[i].head<3>());
    } // head<3>() 是 Eigen 矩阵库提供的一个方法，意思是**“取出这个向量/矩阵的前
      // 3 个元素
    // 相当于把经过透视变换+归一化后的坐标存入三角形

    t.setColor(0, 255.0, 0.0, 0.0);
    t.setColor(1, 0.0, 255.0, 0.0);
    t.setColor(2, 0.0, 0.0, 255.0); // 给三个顶点上色

    rasterize_wireframe(t); // 画线
  }
}

void rst::rasterizer::rasterize_wireframe(const Triangle &t) {
  draw_line(t.c(), t.a());
  draw_line(t.c(), t.b());
  draw_line(t.b(), t.a()); // 画三条线
}

void rst::rasterizer::set_model(const Eigen::Matrix4f &m) { model = m; }

void rst::rasterizer::set_view(const Eigen::Matrix4f &v) { view = v; }

void rst::rasterizer::set_projection(const Eigen::Matrix4f &p, float near,
                                     float far) {
  projection = p;
  zNear = near;
  zFar = far;
}

void rst::rasterizer::clear(rst::Buffers buff) { // 清空上一帧的帧缓存和深度缓存
  if ((buff & rst::Buffers::Color) == rst::Buffers::Color) {
    std::fill(frame_buf.begin(), frame_buf.end(),
              Eigen::Vector3f{0, 0, 0}); // 把屏幕全都涂成一个颜色
  } // 但是这里有人就纳闷了，深度缓存和颜色信息不应该都是二维的吗，他怎么找自己的begin
    // end呢？，看下面
  if ((buff & rst::Buffers::Depth) == rst::Buffers::Depth) {
    std::fill(
        depth_buf.begin(), depth_buf.end(),
        std::numeric_limits<float>::infinity()); // 把深度缓存全都变成无穷大
  }
}

rst::rasterizer::rasterizer(int w, int h) : width(w), height(h) {
  frame_buf.resize(w * h);
  depth_buf.resize(
      w *
      h); // 这里极其的巧妙，咱们定义的时候把帧缓存和深度缓存定义成了一维数组
  // 于是他这里就用宽*高，进而实现了把屏幕这个二维空间压平存储到一维空间里面
}

int rst::rasterizer::get_index(int x, int y) {
  return (height - 1 - y) * width + x;
  /// 通过这个算法计算出像素在一维数组中的位置
  // 还挺巧妙的，因为计算机存储的y值是向下延伸的，所以说这里是height - 1 - y
  // 源代码是这样的 (height-y)*width + x;这样的话会有一个非常致命的bug
  // 你要是读取原点的时候就会发生 height *width +x
  // 可是数组才只有height*width个元素 这样直接就越界了
}

void rst::rasterizer::set_pixel(const Eigen::Vector3f &point,
                                const Eigen::Vector3f &color)
// 这里之所以要传入一个三维向量，是因为后面要搞深度缓冲

{
  // old index: auto ind = point.y() + point.x() * width;
  if (point.x() < 0 || point.x() >= width || point.y() < 0 ||
      point.y() >= height)
    return; // 只画屏幕内的点
  auto ind = (height - point.y()) * width + point.x();
  frame_buf[ind] = color; // 设定帧缓存里面的像素颜色 要一个个像素设置啊草
}
