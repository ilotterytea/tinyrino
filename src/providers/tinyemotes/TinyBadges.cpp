#include "providers/tinyemotes/TinyBadges.hpp"

#include "common/Aliases.hpp"
#include "common/network/NetworkRequest.hpp"
#include "common/network/NetworkResult.hpp"
#include "messages/Emote.hpp"
#include "messages/Image.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QThread>
#include <QUrl>

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace chatterino {

TinyBadges::Badge createBadge(QString id, QString tooltip, QString urlPrefix,
                              QString instanceUrl, QSize baseSize,
                              bool isRoleBadge)
{
    auto emote = Emote{
        .name = EmoteName{},
        .images =
            ImageSet{Image::fromUrl(Url(TINYEMOTES_BADGE_IMG_API.arg(
                                        urlPrefix, instanceUrl, id, "1x")),
                                    1.0, baseSize),
                     Image::fromUrl(Url(TINYEMOTES_BADGE_IMG_API.arg(
                                        urlPrefix, instanceUrl, id, "2x")),
                                    0.5, baseSize * 2),
                     Image::fromUrl(Url(TINYEMOTES_BADGE_IMG_API.arg(
                                        urlPrefix, instanceUrl, id, "3x")),
                                    0.25, baseSize * 4)},
        .tooltip = Tooltip{std::move(tooltip)},
        .homePage = Url{}};

    return {.badge = std::make_shared<const Emote>(std::move(emote)),
            .id = id,
            .isRoleBadge = isRoleBadge};
}

void TinyBadges::load(const QString &instanceUrl)
{
    QString prefix = "https://";
    if (instanceUrl.startsWith("https://") || instanceUrl.startsWith("http://"))
    {
        prefix = "";
    }

    QUrl url(TINYEMOTES_BADGES_API.arg(prefix, instanceUrl));

    NetworkRequest(url)
        .header("Accept", "application/json")
        .onSuccess([this, prefix, instanceUrl](auto result) {
            std::unique_lock lock(this->mutex_);

            auto jsonRoot = result.parseJson();
            this->tgBadges.guard();

            auto jsonData = jsonRoot.value("data").toArray();

            std::unordered_map<QString, std::vector<Badge>> badges;

            auto instanceUserBadges =
                std::find_if(this->userBadges.begin(), this->userBadges.end(),
                             [&instanceUrl](const auto &x) {
                                 return x.first == instanceUrl;
                             });

            if (instanceUserBadges != this->userBadges.end())
            {
                badges = instanceUserBadges->second;
            }

            for (const auto &jsonDataPart : jsonData)
            {
                const auto jsonBadge = jsonDataPart.toObject();

                if (jsonBadge.value("connection").isNull())
                {
                    continue;
                }

                UserId userID = {jsonBadge.value("connection")
                                     .toObject()
                                     .value("alias_id")
                                     .toString()};

                QSize baseSize(18, 18);

                std::optional<TinyBadges::Badge> roleBadge = std::nullopt;
                std::optional<TinyBadges::Badge> customBadge = std::nullopt;

                if (!jsonBadge.value("custom_badge").isNull())
                {
                    auto jsonBadgeData =
                        jsonBadge.value("custom_badge").toObject();
                    QString id = jsonBadgeData.value("id").toString();
                    QString tooltip = jsonBadge.value("username").toString() +
                                      "'s personal badge<br>(" + instanceUrl +
                                      ")";

                    customBadge = createBadge(id, tooltip, prefix, instanceUrl,
                                              baseSize, false);
                }

                if (!jsonBadge.value("role").isNull())
                {
                    auto jsonRole = jsonBadge.value("role").toObject();
                    if (!jsonRole.value("badge").isNull())
                    {
                        auto jsonBadge = jsonRole.value("badge").toObject();
                        QString id = jsonBadge.value("id").toString();
                        QString tooltip = jsonRole.value("name").toString() +
                                          "<br>(" + instanceUrl + ")";
                        roleBadge = createBadge(id, tooltip, prefix,
                                                instanceUrl, baseSize, true);
                    }
                }

                std::vector<Badge> userBadges;

                if (customBadge.has_value())
                {
                    userBadges.push_back(std::move(customBadge.value()));
                }

                if (roleBadge.has_value())
                {
                    userBadges.push_back(std::move(roleBadge.value()));
                }

                badges.emplace(userID.string, userBadges);
            }

            this->userBadges.emplace(instanceUrl, badges);
        })
        .execute();
}

std::optional<TinyBadges::Badge> TinyBadges::getUserBadge(
    const QString &instanceUrl, const UserId &id, bool findRoleBadge) const
{
    auto instanceBadges =
        std::ranges::find_if(this->userBadges, [&instanceUrl](const auto &x) {
            return x.first == instanceUrl;
        });

    if (instanceBadges == this->userBadges.end())
    {
        return std::nullopt;
    }

    auto userBadges =
        std::ranges::find_if(instanceBadges->second, [&id](const auto &x) {
            return x.first == id.string;
        });

    if (userBadges == instanceBadges->second.end())
    {
        return std::nullopt;
    }

    auto badge = std::ranges::find_if(userBadges->second,
                                      [&findRoleBadge](const auto &x) {
                                          return x.isRoleBadge == findRoleBadge;
                                      });

    if (badge == userBadges->second.end())
    {
        return std::nullopt;
    }

    return *badge;
}

}  // namespace chatterino
