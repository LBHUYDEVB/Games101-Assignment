#include "Triangle.hpp"
#include "rasterizer.hpp"
#include <cmath>
#include <eigen3/Eigen/Eigen>
#include <iostream>
#include <opencv2/opencv.hpp>

constexpr double MY_PI = 3.1415926;

Eigen::Matrix4f get_view_matrix(Eigen::Vector3f eye_pos) {
  Eigen::Matrix4f view = Eigen::Matrix4f::Identity();

  Eigen::Matrix4f translate;
  translate << 1, 0, 0, -eye_pos[0], 0, 1, 0, -eye_pos[1], 0, 0, 1, -eye_pos[2],
      0, 0, 0, 1;

  // translate << 1, 0, 0, -eye_pos[0], 0, 1, 0, -eye_pos[1], 0, 0, 1,
  //     -eye_pos[2], 0, 0, 0, 1;
  // 傻逼一样卧槽，他妈的这个源代码写的第一次谁能看懂啊(上边是源码)
  // 这个变换有个问题，他只考虑了相机是平移的，没有想到相机可能有其他的变换方式
  view = translate * view;

  return view; // 返回了视图变换矩阵
}

Eigen::Matrix4f
get_model_matrix(float rotation_angle) // 需要得到模型的绕z轴的旋转矩阵
// 正常来讲旋转矩阵的计算要比这个复杂的多，这里是手下留情了
{
  Eigen::Matrix4f model = Eigen::Matrix4f::Identity();
  float rotation_angle_red = rotation_angle * MY_PI / 180; // 这里也要改成弧度制
  model << cos(rotation_angle_red), -sin(rotation_angle_red), 0, 0,
      sin(rotation_angle_red), cos(rotation_angle_red), 0, 0, 0, 0, 1, 0, 0, 0,
      0, 1;

  return model;
}

Eigen::Matrix4f
get_projection_matrix(float eye_fov, float aspect_ratio, float zNear,
                      float zFar) // 需要得到透视投影矩阵，基本上就是套公式
// 这里传入的是FOV 宽高比 近裁剪面 远裁剪面
{
  // Students will implement this function
  float t =
      -zNear *
      tan(eye_fov * MY_PI / 180 /
          2); // 傻逼一样我靠 c++的tan函数是弧度制的，而且t是近裁面*tanfov
              // 我这里之前写成远裁面了，而且因为咱们默认摄像机的朝向是负半轴
              // 但是这里传入的是正的近裁面远裁面，为了保证矩阵的最后一行不改为0，0，-1，0
              // 这里要取负号（不取负号的话，xyz都会反转，这里看不出来是因为三角形对称）
  // 我一开始还以为是角度制的，怪不得开头定义了MY_PI
  float r = t * aspect_ratio;
  Eigen::Matrix4f projection = Eigen::Matrix4f::Identity(); // 安全初始化内存
  projection << zNear / r, 0, 0, 0, 0, zNear / t, 0, 0, 0, 0,
      (zNear + zFar) / (zNear - zFar), -2 * zNear * zFar / (zNear - zFar), 0, 0,
      1, 0;

  // TODO: Implement this function
  // Create the projection matrix for the given parameters.
  // Then return it.

  return projection;
}

int main(int argc, const char **argv)
// argc代表着传入了几个刹那好苏个数
// 比如说./Rasterizer -r 45 output.png
// 就是 4个参数
// const char **argv 是存放字符串的数组，用来按顺序保存这些被切开的字符串
// argv[0]是./Rasterizer
// argv[1]是-r
// argv[2]是45
// argv[3]是output.png
{
  float angle = 0;
  bool command_line = false;
  std::string filename = "output.png";

  if (argc >= 3) {
    // 只有当你输入了 3 个或更多参数时（比如 ./Rasterizer -r
    // 45）才会进入这里，触发【命令行静默渲染模式】 否则（比如只输入
    // ./Rasterizer），直接跳过整个判断，保持 false 并进入下方的实时窗口模式
    command_line = true;
    angle =
        std::stof(argv[2]); // 提取第 3 个参数（比如
                            // "45"），从字符串转换成浮点数，作为将要旋转的角度

    // 如果想要截图，必须且只能刚刚好输 4 个参数（./Rasterizer -r 45
    // output.png）
    if (argc == 4) {
      filename = std::string(argv[3]); // 提取第 4 个参数作为输出的文件名
    } else {
      // 只要你想进命令行模式，但是没输够 4 个参数（或者输多了），不符合格式要求
      // 这里就会极其无情地直接结束整个 main 函数的操作，退出程序
      return 0;
    }
  }

  rst::rasterizer r(700, 700); // 确认渲染面积

  Eigen::Vector3f eye_pos = {0, 0, 5}; // 相机位置

  std::vector<Eigen::Vector3f> pos{
      {2, 0, -2}, {0, 2, -2}, {-2, 0, -2}}; // 三角形位置

  std::vector<Eigen::Vector3i> ind{
      {0, 1, 2}}; // 三角形索引（后续应该会有详细的算法来区分三角形索引）

  auto pos_id = r.load_positions(pos); // 加载位置
  auto ind_id = r.load_indices(ind);   // 加载索引
  // 草 这里要注意他还真的就是单纯的把一大堆顶点和索引存到字典里的一个位置
  //  具体的读取是靠draw里面的一个函数
  // 这也太抽象了吧，真就是直接塞啊

  int key = 0;
  int frame_count = 0;

  if (command_line) {                                   // 离线渲染
    r.clear(rst::Buffers::Color | rst::Buffers::Depth); // 清空颜色和深度缓冲

    r.set_model(get_model_matrix(angle)); // 设置模型变换矩阵
    r.set_view(get_view_matrix(eye_pos)); // 设置视图变换矩阵
    r.set_projection(get_projection_matrix(45, 1, 0.1, 50), 0.1,
                     50); // 这里同时就传入了近裁面远裁面
    // 防止后续的深度缓冲空间线性变换部分还得硬编码

    r.draw(pos_id, ind_id, rst::Primitive::Triangle); // 画第一个模型
    // 【把内存里的数据变回真正的“图片”】
    // 1. r.frame_buffer().data()
    // 拿到了我们刚才那个一维大长条数组的最底层内存指针
    // 2. cv::Mat 是 OpenCV
    // 里的图片矩阵对象，这里告诉它：请把刚才那个长条数组，按 700x700
    // 重新折叠成正方形！
    // 3. CV_32FC3 意思是：这个数组里的每个元素是 32位小数（Float），且有 3
    // 个通道（RGB）
    cv::Mat image(700, 700, CV_32FC3, r.frame_buffer().data());

    // 4. 但咱们平时看的 PNG 或 JPG 图片不能是 Float 类型，必须是 0~255 的整数。
    // convertTo 的作用就是把 Float
    // 转换为标准图片的存储格式：8位无符号整数（Unsigned Char），3
    // 通道（CV_8UC3）
    image.convertTo(image, CV_8UC3, 1.0f);

    // 5. 用 OpenCV
    // 的写入库把这张符合标准格式的图片发往硬盘保存，名字就是终端里传入的
    // filename
    cv::imwrite(filename, image);

    return 0;
  }

  while (key != 27) { // 只要按键不是27（esc）就一直循环
    r.clear(rst::Buffers::Color | rst::Buffers::Depth); // 清空颜色和深度缓冲

    r.set_model(get_model_matrix(angle)); // 设置模型变换矩阵
    r.set_view(get_view_matrix(eye_pos)); // 设置视图变换矩阵
    r.set_projection(get_projection_matrix(45, 1, 0.1, 50), 0.1,
                     50); // 设置投影矩阵

    r.draw(
        pos_id, ind_id,
        rst::Primitive::Triangle); // 循环加载位置和索引，然后直接收三角形图元

    cv::Mat image(700, 700, CV_32FC3, r.frame_buffer().data());
    image.convertTo(image, CV_8UC3, 1.0f);
    cv::imshow("image", image);
    key = cv::waitKey(
        10); // 等待10ms，如果期间有按键按下，就返回按键的ASCII码，否则返回-1

    std::cout << "frame count: " << frame_count++ << '\n';

    if (key == 'a') {
      angle += 10;
    } else if (key == 'd') {
      angle -= 10;
    }
  }

  return 0;
}
