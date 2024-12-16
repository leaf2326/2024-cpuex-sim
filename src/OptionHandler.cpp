#include "OptionHandler.hpp"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <limits>

namespace po = boost::program_options;

OptionHandler::OptionHandler()
    : inputFilePath("sld/contest.sld"),
      outputRegNum(-1),
      maxStep(DEFAULT_MAX_STEP),
      memorySize(DEFAULT_MEMORY_SIZE),
      enableCache(false),
      enableICount(false),
      enableDebug(false),
      enableGDB(false),
      desc("Command Line Options") {

    // オプション定義
    desc.add_options()
        ("help,h", "Show help message")
        ("input,i", po::value<std::string>(&inputFilePath)->default_value(inputFilePath), "Input file path")
        ("notify,n", po::bool_switch(&enableNotify), "Send notification to Discord Webhook after execution (Webhook URL in 'discordWebhook.txt')")
        ("reg,r", po::value<int>(&outputRegNum)->default_value(outputRegNum), "Specify register to output (0-31: x0-x31, 32-63: fp0-fp31)")
        ("limit,l", po::value<uint64_t>(&maxStep)->default_value(maxStep), "Set max instruction count")
        ("memory,m", po::value<uint64_t>(&memorySize)->default_value(memorySize), "Set DRAM size in MiB")
        ("cache,c", po::bool_switch(&enableCache), "Enable cache memory (may reduce performance)")
        ("icount", po::bool_switch(&enableICount), "Output each instruction's count in memory")
        ("debug,d", po::bool_switch(&enableDebug), "Enable verbose logging (intended for short code execution))")
        ("gdb,g", po::bool_switch(&enableGDB), "Enable GDB-like debugging");
}

// 引数解析メソッド
void OptionHandler::parse(int argc, char* argv[]) {
    try {
        po::variables_map vm;
        auto const parsing_result = po::parse_command_line(argc, argv, desc);
        po::store(parsing_result, vm);

        po::notify(vm);

        // ヘルプオプションが指定された場合
        if (vm.count("help")) {
            std::cerr << desc << std::endl;
            exit(0);
        }

        // デバッグモードの説明
        if (enableDebug) {
            std::cerr << "Debug mode enabled: Verbose logging will be displayed.\n"
                      << "Note: This mode is intended for short code execution due to high output volume.\n";
        }

    for (auto const& str : collect_unrecognized(parsing_result.options, po::include_positional)) {
        programFilePath = str;
        break;
    }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        exit(1);
    }
}
