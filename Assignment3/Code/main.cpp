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
    const fragment_shader_payload &payload) // 这玩意的核心逻辑就是，拿到模型在uv空间的映射，然后把这个uv坐标的rgb值当成kd，然后就再套一遍布林冯模型就行了
{
    Eigen::Vector3f return_color = {0, 0, 0};
    if (payload.texture) {
        return_color = payload.texture->getColor(payload.tex_coords.x(), payload.tex_coords.y());
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

    result_color += ka.cwiseProduct(amb_light_intensity);
    for (auto &light : lights) {
        // 先来算入射光
        Eigen::Vector3f light_dir = light.position - point;
        // 计算观察方向（注意观察方向和视点方向不是一个东西）
        Eigen::Vector3f view_dir = eye_pos - point;
        // 再算半程向量
        Eigen ::Vector3f half_vector =
            (light_dir.normalized() + view_dir.normalized())
                .normalized(); // 注意这里是加法，并且应该先归一化再加
        // 计算到达当前位置的光强度
        float r_square = light_dir.squaredNorm(); // 这个squareNorm的意思是向量长度的平方
        // float r_square = (light_dir.x()*light_dir.x() +
        // light_dir.y()*light_dir.y() + light_dir.z()*light_dir.z()); std::sqrt
        // 上面这个算法是tmd开平方 我是傻逼

        // 计算漫反射光
        Eigen::Vector3f Ld = kd.cwiseProduct(light.intensity / r_square) * std::max(0.0f, normal.dot(light_dir.normalized()));

        // 计算高光
        auto Ls = ks.cwiseProduct(light.intensity / r_square) * std::pow(std::max(0.0f, half_vector.dot(normal.normalized())), p); // 注意这里不能直接用*
                                                                                                                                   // 应该用cwiseProduct，他的意思才是向量间的逐元素相乘

        result_color += Ld + Ls;
        // TODO: For each light source in the code, calculate what the *ambient*,
        // *diffuse*, and *specular* compone
        // nts are. Then, accumulate that result on
        // the *result_color* object.
    }

    return result_color * 255.f;
}

Eigen::Vector3f phong_fragment_shader(const fragment_shader_payload &payload) {
    Eigen::Vector3f ka = Eigen::Vector3f(0.005, 0.005, 0.005); // 环境光系数
    Eigen::Vector3f kd = payload.color;                        // 漫反射系数/物体固有色
    Eigen::Vector3f ks = Eigen::Vector3f(
        0.7937, 0.7937, 0.7937); // 高光系数，判断什么条件会产生高光

    auto l1 = light{
        {20, 20, 20},
        {500, 500, 500}};                           // 妈的这里是在填充光源的结构体 这里对应的是光源位置
    auto l2 = light{{-20, 20, 0}, {500, 500, 500}}; // 光源强度

    std::vector<light> lights = {l1, l2};
    Eigen::Vector3f amb_light_intensity{10, 10, 10};
    Eigen::Vector3f eye_pos{0, 0, 10};

    float p = 150;

    Eigen::Vector3f color = payload.color;
    Eigen::Vector3f point = payload.view_pos;
    Eigen::Vector3f normal = payload.normal;

    Eigen::Vector3f result_color = {0, 0, 0};
    result_color += ka.cwiseProduct(amb_light_intensity);
    for (auto &light : lights) { // 这里要连着计算整个phone模型所需要的变量，而且这里是对于每个光源来讲
        //  // 所以说环境光应该放在循环外，因为他只加一次（不对 不应该只加一次）

        // 先来算入射光
        Eigen::Vector3f light_dir = light.position - point;
        // 计算观察方向（注意观察方向和视点方向不是一个东西）
        Eigen::Vector3f view_dir = eye_pos - point;
        // 再算半程向量
        Eigen ::Vector3f half_vector =
            (light_dir.normalized() + view_dir.normalized())
                .normalized(); // 注意这里是加法，并且应该先归一化再加
        // 计算到达当前位置的光强度
        float r_square = light_dir.squaredNorm(); // 这个squareNorm的意思是向量长度的平方
        // float r_square = (light_dir.x()*light_dir.x() +
        // light_dir.y()*light_dir.y() + light_dir.z()*light_dir.z()); std::sqrt
        // 上面这个算法是tmd开平方 我是傻逼

        // 计算漫反射光
        Eigen::Vector3f Ld = kd.cwiseProduct(light.intensity / r_square) * std::max(0.0f, normal.dot(light_dir.normalized()));

        // 计算高光
        auto Ls = ks.cwiseProduct(light.intensity / r_square) * std::pow(std::max(0.0f, half_vector.dot(normal.normalized())), p); // 注意这里不能直接用*
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

    float kh = 0.2, // 控制高度图本身的的高度幅度，主要影响法线扰动的强弱；值越大，表面看起来越崎岖。
        kn = 0.1;   // 控制最终置换结果的高度缩放倍率，同时也会间接影响凹凸的视觉强度

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
    if (payload.texture) {                       // 如果当前 shader payload 带有高度图纹理，就执行置换贴图逻辑
        Eigen::Vector3f n = normal.normalized(); // 把插值得到的原始法线归一化，作为后续 TBN 坐标系里的 n 轴
        float x = n.x();                         // 取出归一化法线的 x 分量，对应公式中的 n.x
        float y = n.y();                         // 取出归一化法线的 y 分量，对应公式中的 n.y
        float z = n.z();                         // 取出归一化法线的 z 分量，对应公式中的 n.z
        float xz_length = sqrt(x * x + z * z);   // 计算 sqrt(x*x+z*z)，这是构造 tangent 时的分母

        Eigen::Vector3f t;                                                        // 声明切线向量 t，也就是 tangent，表示纹理 u 方向在观察空间里的方向
        if (xz_length > 1e-6f) {                                                  // 如果分母不是接近 0，就按作业公式正常构造切线
            t = Eigen::Vector3f(x * y / xz_length, xz_length, z * y / xz_length); // 根据 n 构造切线 t
        } else {                                                                  // 如果 x 和 z 都接近 0，公式会除以很小的数，所以走备用分支
            t = Eigen::Vector3f(1.0f, 0.0f, 0.0f);                                // 选 x 轴作为备用切线，保证数值稳定
        } // 切线 t 构造完成
        Eigen::Vector3f b = n.cross(t); // 用 n 叉乘 t 得到副切线 b，也就是 bitangent

        Eigen::Matrix3f TBN;        // 声明 TBN 矩阵，用来把切线空间的法线变换回观察空间
        TBN << t.x(), b.x(), n.x(), // 第一行是 t、b、n 三个基向量的 x 分量
            t.y(), b.y(), n.y(),    // 第二行是 t、b、n 三个基向量的 y 分量
            t.z(), b.z(), n.z();    // 第三行是 t、b、n 三个基向量的 z 分量

        float u = payload.tex_coords.x();                               // 取当前片元的纹理坐标 u
        float v = payload.tex_coords.y();                               // 取当前片元的纹理坐标 v
        float w = payload.texture->width;                               // 取高度图宽度，用来计算 u 方向一个 texel 的步长 1/w
        float h = payload.texture->height;                              // 取高度图高度，用来计算 v 方向一个 texel 的步长 1/h
        float current_height = payload.texture->getColor(u, v).norm();  // 当前 uv 位置的高度值，这里用颜色向量长度近似高度
        float dU = kh * kn *                                            // 计算 u 方向的高度变化，并乘上高度缩放系数 kh 和法线缩放系数 kn
                   (payload.texture->getColor(u + 1.0f / w, v).norm() - // 采样 u 方向右侧一个 texel 的高度 h(u+1/w,v)
                    current_height);                                    // 减去当前高度 h(u,v)，得到 u 方向高度差
        float dV = kh * kn *                                            // 计算 v 方向的高度变化，并乘上高度缩放系数 kh 和法线缩放系数 kn
                   (payload.texture->getColor(u, v + 1.0f / h).norm() - // 采样 v 方向上方一个 texel 的高度 h(u,v+1/h)
                    current_height);                                    // 减去当前高度 h(u,v)，得到 v 方向高度差

        Eigen::Vector3f ln(-dU, -dV, 1.0f);      // 在切线空间里构造扰动后的局部法线 ln=(-dU,-dV,1)
        point = point + kn * n * current_height; // 置换贴图比 bump 多这一步：沿原始法线方向移动当前 shading point
        // 新位置 = 原位置 + 法线方向 * 高度值 * 缩放系数

        normal = (TBN * ln).normalized(); // 把局部扰动法线通过 TBN 转回观察空间，并归一化
    } // 置换贴图的高度采样、位置移动、法线扰动结束

    Eigen::Vector3f result_color = {0, 0, 0};             // 初始化最终颜色累加器
    result_color += ka.cwiseProduct(amb_light_intensity); // 加一次环境光，环境光不应该随光源数量重复

    for (auto &light : lights) {                            // 遍历每一个点光源，分别计算漫反射和高光
        Eigen::Vector3f light_dir = light.position - point; // 从当前点指向光源的向量
        Eigen::Vector3f view_dir = eye_pos - point;         // 从当前点指向相机的观察向量
        Eigen::Vector3f half_vector =
            (light_dir.normalized() + view_dir.normalized()).normalized(); // Blinn-Phong 的半程向量 h=normalize(l+v)
        float r_square = light_dir.squaredNorm();                          // 光源到当前点距离的平方，用于做距离衰减

        Eigen::Vector3f Ld =
            kd.cwiseProduct(light.intensity / r_square) *                    // 漫反射项：物体颜色 kd 逐通道乘以衰减后的光强
            std::max(0.0f, normal.normalized().dot(light_dir.normalized())); // 再乘 max(0,n·l)，背光面不贡献漫反射

        Eigen::Vector3f Ls =
            ks.cwiseProduct(light.intensity / r_square) *                      // 高光项：高光系数 ks 逐通道乘以衰减后的光强
            std::pow(std::max(0.0f, half_vector.dot(normal.normalized())), p); // 再乘 max(0,n·h)^p，p 越大高光越尖

        result_color += Ld + Ls; // 把当前光源的漫反射和高光贡献累加到最终颜色
    } // 所有光源处理完毕

    return result_color * 255.f; // 内部按 0~1 颜色计算，返回时放大到 0~255
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

    // 这里来说一下逻辑，先构建一个由法线+切线+副切线（法线和切线的叉乘（妈的这不是构建空间坐标系常用到的叉乘了，我怎么忘了草））
    // 组成的TBN空间（tangent/bitangent/normal），然后利用高度图 sample 采样出当前uv的
    //
    if (payload.texture) {                       // 如果当前 shader payload 带有高度图纹理，就执行 bump mapping
        Eigen::Vector3f n = normal.normalized(); // 把原始插值法线归一化，作为 TBN 坐标系里的 n 轴
        float x = n.x();                         // 取出法线 x 分量
        float y = n.y();                         // 取出法线 y 分量
        float z = n.z();                         // 取出法线 z 分量
        float xz_length = sqrt(x * x + z * z);   // 计算 sqrt(x*x+z*z)，用于构造切线向量

        Eigen::Vector3f t;                                                        // 声明切线向量 t，也就是 tangent
        if (xz_length > 1e-6f) {                                                  // 分母足够大时，使用作业给出的切线公式
            t = Eigen::Vector3f(x * y / xz_length, xz_length, z * y / xz_length); // 根据原始法线构造切线 t
        } else {                                                                  // 分母接近 0 时，公式会不稳定
            t = Eigen::Vector3f(1.0f, 0.0f, 0.0f);                                // 使用 x 轴方向作为备用切线
        } // 切线 t 构造完成
        Eigen::Vector3f b = n.cross(t); // 用 n 叉乘 t 得到副切线 b

        Eigen::Matrix3f TBN;        // 声明 TBN 矩阵，用于把切线空间法线转回观察空间
        TBN << t.x(), b.x(), n.x(), // 第一行：t、b、n 三个基向量的 x 分量
            t.y(), b.y(), n.y(),    // 第二行：t、b、n 三个基向量的 y 分量
            t.z(), b.z(), n.z();    // 第三行：t、b、n 三个基向量的 z 分量
                                    // 这里讲一下这个TBN矩阵，他实际上是把法线，切线，副切线这三个局部空间的基向量用世界空间的坐标表示了
        // 就有点类似于世界模型矩阵，在模型内部各个基向量肯定是001之类的，但是用世界空间的角度去表示就成每个模型在世界空间的坐标
        // 这样的模型矩阵可以把每个模型内部的坐标转换到世界空间坐标
        // 同理，TBN也可以把切线空间法线转换到世界空间坐标
        float u = payload.tex_coords.x();  // 当前片元的纹理坐标 u
        float v = payload.tex_coords.y();  // 当前片元的纹理坐标 v
        float w = payload.texture->width;  // 高度图宽度，用于计算 u 方向一个 texel 的步长
        float h = payload.texture->height; // 高度图高度，用于计算 v 方向一个 texel 的步长
        // 之前的getcolor函数的逻辑是把uv空间和贴图的像素一一对应
        // 这里的计算步长，是为了计算高度图的变化率（前后像素高度差对比，具体为什么要通过这种方法算法线请见lecture8笔记）
        float current_height = payload.texture->getColor(u, v).norm();  // 当前 uv 位置的高度值
        float dU = kh * kn *                                            // 计算 u 方向高度变化，并乘上 kh、kn 控制凹凸强度
                   (payload.texture->getColor(u + 1.0f / w, v).norm() - // 采样右侧相邻 texel 的高度
                    current_height);                                    // 用右侧高度减当前高度，得到 u 方向高度差
        float dV = kh * kn *                                            // 计算 v 方向高度变化，并乘上 kh、kn 控制凹凸强度
                   (payload.texture->getColor(u, v + 1.0f / h).norm() - // 采样上方相邻 texel 的高度
                    current_height);                                    // 用上方高度减当前高度，得到 v 方向高度差

        Eigen::Vector3f ln(-dU, -dV, 1.0f); // 在切线空间里构造扰动后的局部法线 ln=(-dU,-dV,1)，这里是直接套的公式，推导过程自己看笔记
        normal = (TBN * ln).normalized();   // 把局部扰动法线乘 TBN 转回观察空间，并归一化
    } // bump mapping 结束；它只改 normal，不移动 point

    Eigen::Vector3f result_color = {0, 0, 0}; // 初始化返回颜色
    result_color = normal;                    // bump shader 直接把扰动后的法线当颜色显示

    return result_color * 255.f; // 把颜色从 0~1 范围放大到 0~255 输出
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
