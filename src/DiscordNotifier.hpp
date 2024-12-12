#ifndef DISCORD_NOTIFIER_HPP
#define DISCORD_NOTIFIER_HPP

#include <string>

void sendDiscordNotification(const std::string& message);

std::string readWebhookUrl(const std::string& filePath);

#endif
