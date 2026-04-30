#pragma once
//
// 启动装配：集中配置 Drogon 在运行前需要的初始化钩子，
// 后续要加全局拦截器、信号处理、预热逻辑都放这里。
//
namespace core {

class Bootstrap {
public:
    // 读取 config/config.json 并执行启动前的初始化
    static void init(const std::string& configPath);
};

} // namespace core
