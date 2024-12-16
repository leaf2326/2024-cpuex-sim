#include "Simulator.hpp"
#include "DiscordNotifier.hpp"
#include "OptionHandler.hpp"
#include <iostream>
#include <chrono>

int main(int argc, char *argv[])
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::cerr << std::showbase;

    OptionHandler options;
    options.parse(argc, argv);

    std::cerr << "Input file path: " << options.inputFilePath << std::endl;
    std::cerr << "Max steps: " << options.maxStep << std::endl;
    std::cerr << "Memory size: " << options.memorySize/1024/1024 << " MiB\n";

    if (options.enableCache) {
        std::cerr << "Cache memory enabled.\n";
    }
    if (options.enableICount) {
        std::cerr << "Instruction count output enabled.\n";
    }
    if (options.enableDebug) {
        std::cerr << "Debug mode enabled.\n";
    }
    if (options.enableGDB) {
        std::cerr << "GDB-like debugging enabled.\n";
    }

    Simulator simulator(options);
    // 開始日時を取得

    auto start = std::chrono::system_clock::now();
    bool runningProgram = false;
    // プログラムシミュレートの準備
    try
    {
        simulator.loadInputData(options.inputFilePath);
        simulator.loadMemoryFromBinary(options.programFilePath);

        // シミュレート開始
        runningProgram = true;
        simulator.runProgram(options.outputRegNum);
        auto end = std::chrono::system_clock::now();
        if (not options.enableGDB)
            simulator.printLog();

        std::cerr << "________Simulator Terminated________" << std::endl;
        std::chrono::duration<double, std::milli> elapsed = end - start;

        // end - start を秒単位で計算
        std::chrono::duration<double> elapsed2 = end - start;
        std::cerr << "Simulator Execution time: " << elapsed.count() / 1000.0 << "sec" << std::endl;
        if (options.enableNotify)
        {
            sendDiscordNotification("Simulator Execution time: " + std::to_string(elapsed.count() / 1000.0) + "sec");
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        if (runningProgram)
        {
            std::cerr << "__DEBUG INFO__ " << std::endl;
            simulator.printLog();
        }
        if (options.enableNotify)
        {
            sendDiscordNotification(std::string("Simulator Error: ") + e.what());
        }
        return 1;
    }
    return 0;
}