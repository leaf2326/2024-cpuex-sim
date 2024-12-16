#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "DiscordNotifier.hpp"
#include "../include/httplib.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

std::string readWebhookUrl(const std::string &filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        throw std::runtime_error("can't open webhook file: " + filePath);
    }

    std::string webhookUrl;
    std::getline(file, webhookUrl);
    file.close();

    if (webhookUrl.empty())
    {
        throw std::runtime_error("No webhook URL in specified file.");
    }

    return webhookUrl;
}

// Discord通知を送信
void sendDiscordNotification(const std::string &message)
{
    try
    {
        const std::string webhookUrl = readWebhookUrl("discordWebhook.txt");

        std::string host = "discord.com";
        std::string path = webhookUrl.substr(webhookUrl.find("/api/"));

        httplib::SSLClient cli(host.c_str());
        cli.set_follow_location(true);

        std::string payload = R"({"content": ")" + message + R"("})";

        auto res = cli.Post(path.c_str(), payload, "application/json");

        if (res && res->status == 204)
        {
            std::cerr << "Notification sent successfully!" << std::endl;
        }
        else
        {
            std::cerr << "Failed to send notification." << std::endl;
            if (res)
            {
                std::cerr << "Status code: " << res->status << std::endl;
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}
