#include <iostream>
#include <opencv2/opencv.hpp>

#include "OBJ_Loader.h"
#include "Shader.hpp"
#include "Texture.hpp"
#include "Triangle.hpp"
#include "global.hpp"
#include "rasterizer.hpp"

Eigen::Matrix4f get_view_matrix(Eigen::Vector3f eye_pos) {
  Eigen::Matrix4f view = Eigen::Matrix4f::Identity();

  Eigen::Matrix4f translate;
  translate << 1, 0, 0, -eye_pos[0], 0, 1, 0, -eye_pos[1], 0, 0, 1, -eye_pos[2],
      0, 0, 0, 1;

  view = translate * view;

  return view;
}

Eigen::Matrix4f get_model_matrix(float angle) {
  Eigen::Matrix4f rotation;
  angle = angle * MY_PI / 180.f;
  rotation << cos(angle), 0, sin(angle), 0, 0, 1, 0, 0, -sin(angle), 0,
      cos(angle), 0, 0, 0, 0, 1;

  Eigen::Matrix4f scale;
  scale << 2.5, 0, 0, 0, 0, 2.5, 0, 0, 0, 0, 2.5, 0, 0, 0, 0, 1;

  Eigen::Matrix4f translate;
  translate << 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1;

  return translate * rotation * scale;
}

Eigen::Matrix4f get_projection_matrix(float eye_fov, float aspect_ratio,
                                      float zNear, float zFar) {
  Eigen::Matrix4f projection = Eigen::Matrix4f::Identity();
  float t = -zNear * tan(eye_fov * MY_PI / 180.0f / 2.0f);
  float r = t * aspect_ratio;

  projection << zNear / r, 0, 0, 0, 0, zNear / t, 0, 0, 0, 0,
      (zNear + zFar) / (zNear - zFar), -2 * zNear * zFar / (zNear - zFar), 0, 0,
      1, 0;
  return projection; //   妈的这里还要补上透视投影矩阵
}

Eigen::Vector3f vertex_shader(const vertex_shader_payload &payload) {
  return payload.position; // 虚职 没有任何调用
}

Eigen::Vector3f normal_fragment_shader(const fragment_shader_payload &payload) {
  Eigen::Vector3f return_color =
      (payload.normal.head<3>().normalized()
       // 归一化
       + Eigen::Vector3f(1.0f, 1.0f, 1.0f)) // 范围变成【0，2】
      / 2.f;                                // 范围变成【0，1】
  // 上面这部实际上就是在把法向量数据变为可以直接乘以RGB的格式

  Eigen::Vector3f result;
  result << return_color.x() * 255, return_color.y() * 255,
      return_color.z() * 255;
  return result;
}

static Eigen::Vector3f reflect(const Eigen::Vector3f &vec,
                               const Eigen::Vector3f &axis) {
  auto costheta = vec.dot(axis);
  return (2 * costheta * axis - vec)
      .normalized(); // 计算反射逻辑，详细推导请见笔记
}

struct light {
  Eigen::Vector3f position;  // 光源位置
  Eigen::Vector3f intensity; // 光源强度
};

Eigen::Vector3f texture_fragment_shader(
    const fragment_shader_payload &
        payload) // 这玩意的核心逻辑就是，拿到模型在uv空间的映射，然后把这个uv坐标的rgb值当成kd，然后就再套一遍布林冯模型就行了
{
  Eigen::Vector3f return_color = {0, 0, 0};
  if (payload.texture) {
    float u_step = 1.0f / static_cast<float>(payload.texture->width);
    float v_step = 1.0f / static_cast<float>(payload.texture->height);
    float u = std::max(0.0f, std::min(payload.tex_coords.x(), 1.0f - u_step));
    float v = std::max(0.0f, std::min(payload.tex_coords.y(), 1.0f - v_step));
    return_color = payload.texture->getColor(u, v); // 0~255

    // TODO: Get the texture value at the texture coordinates of the current
    // fragment
  }
  Eigen::Vector3f texture_color;
  texture_color << return_color.x(), return_color.y(), return_color.z();

  Eigen::Vector3f ka = Eigen::Vector3f(0.005, 0.005, 0.005);
  Eigen::Vector3f kd = texture_color / 255.f;
  Eigen::Vector3f ks = Eigen::Vector3f(0.7937, 0.7937, 0.7937);

  auto l1 = light{{20, 20, 20}, {500, 500, 500}};
  auto l2 = light{{-20, 20, 0}, {500, 500, 500}};

  std::vector<light> lights = {l1, l2};
  Eigen::Vector3f amb_light_intensity{10, 10, 10};
  Eigen::Vector3f eye_pos{0, 0, 10};

  float p = 150;

  Eigen::Vector3f color = texture_color;
  Eigen::Vector3f point = payload.view_pos;
  Eigen::Vector3f normal = payload.normal;

  Eigen::Vector3f result_color = {0, 0, 0};

  result_color = ka.cwiseProduct(amb_light_intensity);
  for (auto &light : lights) {
    // TODO: For each light source in the code, calculate what the *ambient*,
    // *diffuse*, and *specular* components are. Then, accumulate that result on
    // the *result_color* object.
  }

  return result_color * 255.f;
}

Eigen::Vector3f phong_fragment_shader(const fragment_shader_payload &payload) {
  Eigen::Vector3f ka = Eigen::Vector3f(0.005, 0.005, 0.005); // 环境光系数
  Eigen::Vector3f kd = payload.color; // 漫反射系数/物体固有色
  Eigen::Vector3f ks = Eigen::Vector3f(
      0.7937, 0.7937, 0.7937); // 高光系数，判断什么条件会产生高光

  auto l1 = light{
      {20, 20, 20},
      {500, 500, 500}}; // 妈的这里是在填充光源的结构体 这里对应的是光源位置
  auto l2 = light{{-20, 20, 0}, {500, 500, 500}}; // 光源强度

  std::vector<light> lights = {l1, l2};
  Eigen::Vector3f amb_light_intensity{10, 10, 10};
  Eigen::Vector3f eye_pos{0, 0, 10};

  float p = 150;

  Eigen::Vector3f color = payload.color;
  Eigen::Vector3f point = payload.view_pos;
  Eigen::Vector3f normal = payload.normal;

  Eigen::Vector3f result_color = ka.cwiseProduct(amb_light_intensity);
  for (
      auto &light :
      lights) { // 这里要连着计算整个phone模型所需要的变量，而且这里是对于每个光源来讲
    // 所以说环境光应该放在循环外，因为他只加一次

    // 先来算入射光
    Eigen::Vector3f light_dir = light.position - point;
    // 计算观察方向（注意观察方向和视点方向不是一个东西）
    Eigen::Vector3f view_dir = eye_pos - point;
    // 再算半程向量
    Eigen ::Vector3f half_vector =
        (light_dir.normalized() + view_dir.normalized())
            .normalized(); // 注意这里是加法，并且应该先归一化再加
    // 计算到达当前位置的光强度
    float r_square =
        light_dir.squaredNorm(); // 这个squareNorm的意思是向量长度的平方
    // float r_square = (light_dir.x()*light_dir.x() +
    // light_dir.y()*light_dir.y() + light_dir.z()*light_dir.z()); std::sqrt
    // 是tmd开平方 我是傻逼 计算漫反射光
    Eigen::Vector3f Ld = kd.cwiseProduct(light.intensity / r_square) *
                         std::max(0.0f, normal.dot(light_dir.normalized()));
    // 计算高光
    auto Ls = ks.cwiseProduct(light.intensity / r_square) *
              std::pow(std::max(0.0f, half_vector.dot(normal.normalized())),
                       p); // 注意这里不能直接用*
                           // 应该用cwiseProduct，他的意思才是向量间的逐元素相乘

    result_color += Ld + Ls;
    // TODO: For each light source in the code, calculate what the *ambient*,
    // *diffuse*, and *specular* components are. Then, accumulate that result on
    // the *result_color* object.
  }

  return result_color * 255.f;
}

Eigen::Vector3f
displacement_fragment_shader(const fragment_shader_payload &payload) {

  Eigen::Vector3f ka = Eigen::Vector3f(0.005, 0.005, 0.005);
  Eigen::Vector3f kd = payload.color;
  Eigen::Vector3f ks = Eigen::Vector3f(0.7937, 0.7937, 0.7937);

  auto l1 = light{{20, 20, 20}, {500, 500, 500}};
  auto l2 = light{{-20, 20, 0}, {500, 500, 500}};

  std::vector<light> lights = {l1, l2};
  Eigen::Vector3f amb_light_intensity{10, 10, 10};
  Eigen::Vector3f eye_pos{0, 0, 10};

  float p = 150;

  Eigen::Vector3f color = payload.color;
  Eigen::Vector3f point = payload.view_pos;
  Eigen::Vector3f normal = payload.normal;

  float kh = 0.2, kn = 0.1;

  // TODO: Implement displacement mapping here
  // Let n = normal = (x, y, z)
  // Vector t = (x*y/sqrt(x*x+z*z),sqrt(x*x+z*z),z*y/sqrt(x*x+z*z))
  // Vector b = n cross product t
  // Matrix TBN = [t b n]
  // dU = kh * kn * (h(u+1/w,v)-h(u,v))
  // dV = kh * kn * (h(u,v+1/h)-h(u,v))
  // Vector ln = (-dU, -dV, 1)
  // Position p = p + kn * n * h(u,v)
  // Normal n = normalize(TBN * ln)

  Eigen::Vector3f result_color = {0, 0, 0};

  for (auto &light : lights) {
    // TODO: For each light source in the code, calculate what the *ambient*,
    // *diffuse*, and *specular* components are. Then, accumulate that result on
    // the *result_color* object.
  }

  return result_color * 255.f;
}

Eigen::Vector3f bump_fragment_shader(const fragment_shader_payload &payload) {

  Eigen::Vector3f ka = Eigen::Vector3f(0.005, 0.005, 0.005);
  Eigen::Vector3f kd = payload.color;
  Eigen::Vector3f ks = Eigen::Vector3f(0.7937, 0.7937, 0.7937);

  auto l1 = light{{20, 20, 20}, {500, 500, 500}};
  auto l2 = light{{-20, 20, 0}, {500, 500, 500}};

  std::vector<light> lights = {l1, l2};
  Eigen::Vector3f amb_light_intensity{10, 10, 10};
  Eigen::Vector3f eye_pos{0, 0, 10};

  float p = 150;

  Eigen::Vector3f color = payload.color;
  Eigen::Vector3f point = payload.view_pos;
  Eigen::Vector3f normal = payload.normal;

  float kh = 0.2, kn = 0.1;

  // TODO: Implement bump mapping here
  // Let n = normal = (x, y, z)
  // Vector t = (x*y/sqrt(x*x+z*z),sqrt(x*x+z*z),z*y/sqrt(x*x+z*z))
  // Vector b = n cross product t
  // Matrix TBN = [t b n]
  // dU = kh * kn * (h(u+1/w,v)-h(u,v))
  // dV = kh * kn * (h(u,v+1/h)-h(u,v))
  // Vector ln = (-dU, -dV, 1)
  // Normal n = normalize(TBN * ln)

  Eigen::Vector3f result_color = {0, 0, 0};
  result_color = normal;

  return result_color * 255.f;
}

int main(int argc, const char **argv) {
  std::vector<Triangle *> TriangleList;

  float angle = 140.0;
  bool command_line = false;

  std::string filename = "output.png";
  objl::Loader Loader;
  std::string obj_path = "../models/spot/";

  // Load .obj File
  bool loadout = Loader.LoadFile("../models/spot/spot_triangulated_good.obj");
  for (auto mesh : Loader.LoadedMeshes) {
    for (int i = 0; i < mesh.Vertices.size(); i += 3) {
      Triangle *t = new Triangle();
      for (int j = 0; j < 3; j++) {
        t->setVertex(j, Vector4f(mesh.Vertices[i + j].Position.X,
                                 mesh.Vertices[i + j].Position.Y,
                                 mesh.Vertices[i + j].Position.Z, 1.0));
        t->setNormal(j, Vector3f(mesh.Vertices[i + j].Normal.X,
                                 mesh.Vertices[i + j].Normal.Y,
                                 mesh.Vertices[i + j].Normal.Z));
        t->setTexCoord(j, Vector2f(mesh.Vertices[i + j].TextureCoordinate.X,
                                   mesh.Vertices[i + j].TextureCoordinate.Y));
      }
      TriangleList.push_back(t);
    }
  }

  rst::rasterizer r(700, 700);

  auto texture_path = "hmap.jpg";
  r.set_texture(Texture(obj_path + texture_path));

  std::function<Eigen::Vector3f(fragment_shader_payload)> active_shader =
      phong_fragment_shader;

  if (argc >= 2) { // 在这里通过命令行参数来选择不同的shader
    command_line = true;
    filename = std::string(argv[1]);

    if (argc == 3 && std::string(argv[2]) == "texture") {
      std::cout << "Rasterizing using the texture shader\n";
      active_shader = texture_fragment_shader;
      texture_path = "spot_texture.png";
      r.set_texture(Texture(obj_path + texture_path));
    } else if (argc == 3 && std::string(argv[2]) == "normal") {
      std::cout << "Rasterizing using the normal shader\n";
      active_shader = normal_fragment_shader;
    } else if (argc == 3 && std::string(argv[2]) == "phong") {
      std::cout << "Rasterizing using the phong shader\n";
      active_shader = phong_fragment_shader;
    } else if (argc == 3 && std::string(argv[2]) == "bump") {
      std::cout << "Rasterizing using the bump shader\n";
      active_shader = bump_fragment_shader;
    } else if (argc == 3 && std::string(argv[2]) == "displacement") {
      std::cout << "Rasterizing using the bump shader\n";
      active_shader = displacement_fragment_shader;
    }
  }

  Eigen::Vector3f eye_pos = {0, 0, 10};

  r.set_vertex_shader(vertex_shader);
  r.set_fragment_shader(active_shader); // shader存入光栅器

  int key = 0;
  int frame_count = 0;

  if (command_line) {
    r.clear(rst::Buffers::Color | rst::Buffers::Depth);
    r.set_model(get_model_matrix(angle));
    r.set_view(get_view_matrix(eye_pos));
    r.set_projection(get_projection_matrix(45.0, 1, 0.1, 50));

    r.draw(TriangleList);
    cv::Mat image(700, 700, CV_32FC3, r.frame_buffer().data());
    image.convertTo(image, CV_8UC3, 1.0f);
    cv::cvtColor(image, image, cv::COLOR_RGB2BGR);

    cv::imwrite(filename, image);

    return 0;
  }

  while (key != 27) {
    r.clear(rst::Buffers::Color | rst::Buffers::Depth);

    r.set_model(get_model_matrix(angle));
    r.set_view(get_view_matrix(eye_pos));
    r.set_projection(get_projection_matrix(45.0, 1, 0.1, 50));

    // r.draw(pos_id, ind_id, col_id, rst::Primitive::Triangle);
    r.draw(TriangleList);
    cv::Mat image(700, 700, CV_32FC3, r.frame_buffer().data());
    image.convertTo(image, CV_8UC3, 1.0f);
    cv::cvtColor(image, image, cv::COLOR_RGB2BGR);

    cv::imshow("image", image);
    cv::imwrite(filename, image);
    key = cv::waitKey(10);

    if (key == 'a') {
      angle -= 0.1;
    } else if (key == 'd') {
      angle += 0.1;
    }
  }
  return 0;
}
