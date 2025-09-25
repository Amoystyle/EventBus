@echo off

echo ==========================================
echo    EventBus 企业级演示程序
echo ==========================================
echo.
echo 这个演示将展示EventBus的所有核心功能:
echo    - 线程安全的事件处理
echo    - 智能类型转换(const char* → std::string)
echo    - 任意参数数量支持
echo    - 高性能事件分发
echo    - 完整的统计监控
echo.
pause

echo.
echo [1/3] 基础功能演示
echo ----------------------
echo 运行简单测试，展示基本订阅和发布功能...
echo.
simple_test.exe

echo.
echo [2/3] 实际应用演示  
echo ----------------------
echo 运行实际使用示例，展示企业级应用场景...
echo.
usage_example.exe

echo.
echo [3/3] 完整功能演示
echo ----------------------
echo 运行完整测试，包含性能和线程安全测试...
echo 注意：这个测试会运行性能基准和多线程测试
echo.
pause
complete_test.exe

echo.
echo ==========================================
echo    EventBus 演示完成！
echo ==========================================
echo.
echo  恭喜!您已经看到了EventBus的全部能力:
echo.
echo    - 线程安全的并发事件处理
echo    - 智能的自动类型转换
echo    - 高性能的零开销抽象
echo    - 企业级的监控和统计
echo    - 灵活的订阅管理
echo.
echo  更多信息请查看：
echo    - README.md - 完整使用文档
echo    - PROJECT_SUMMARY.md - 项目技术总结
echo    - eventbus.hpp - 源码实现
echo.
echo  您的EventBus已经准备好用于生产环境!
echo.
pause
