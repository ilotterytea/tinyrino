#pragma once

#include "util/RapidjsonHelpers.hpp"

#include <pajlada/serialize.hpp>
#include <QString>
#include <QUrl>

#include <memory>

namespace chatterino {

class Image;
using ImagePtr = std::shared_ptr<Image>;

class TinyemotesInstance
{
public:
    TinyemotesInstance(const QString &url, bool enableGlobalEmotes,
                       bool enableChannelEmotes, bool enableAvatars,
                       bool enableBadges);

    bool operator==(const TinyemotesInstance &other) const;

    const QString &getUrl() const;
    bool isGlobalEmotesEnabled() const;
    bool isChannelEmotesEnabled() const;
    bool isAvatarEnabled() const;
    bool isBadgeEnabled() const;

private:
    QString url_;
    bool globalEmotesEnabled_, channelEmotesEnabled_, avatarsEnabled_,
        badgesEnabled_;
};

}  // namespace chatterino

namespace pajlada {

template <>
struct Serialize<chatterino::TinyemotesInstance> {
    static rapidjson::Value get(const chatterino::TinyemotesInstance &value,
                                rapidjson::Document::AllocatorType &a)
    {
        rapidjson::Value ret(rapidjson::kObjectType);

        chatterino::rj::set(ret, "url", value.getUrl(), a);
        chatterino::rj::set(ret, "channelEmotes",
                            value.isChannelEmotesEnabled(), a);
        chatterino::rj::set(ret, "globalEmotes", value.isGlobalEmotesEnabled(),
                            a);
        chatterino::rj::set(ret, "avatars", value.isAvatarEnabled(), a);
        chatterino::rj::set(ret, "badges", value.isBadgeEnabled(), a);

        return ret;
    }
};

template <>
struct Deserialize<chatterino::TinyemotesInstance> {
    static chatterino::TinyemotesInstance get(const rapidjson::Value &value,
                                              bool *error = nullptr)
    {
        if (!value.IsObject())
        {
            PAJLADA_REPORT_ERROR(error)
            return chatterino::TinyemotesInstance(QString(), false, false,
                                                  false, false);
        }

        QString url;
        chatterino::rj::getSafe(value, "url", url);

        bool enableChannelEmotes;
        chatterino::rj::getSafe(value, "channelEmotes", enableChannelEmotes);

        bool enableGlobalEmotes;
        chatterino::rj::getSafe(value, "globalEmotes", enableGlobalEmotes);

        bool enableAvatars;
        if (value.HasMember("avatars"))
        {
            chatterino::rj::getSafe(value, "avatars", enableAvatars);
        }
        else
        {
            enableAvatars = true;
        }

        bool enableBadges = true;
        if (value.HasMember("badges"))
        {
            chatterino::rj::getSafe(value, "badges", enableBadges);
        }

        return chatterino::TinyemotesInstance(url, enableGlobalEmotes,
                                              enableChannelEmotes,
                                              enableAvatars, enableBadges);
    }
};

}  // namespace pajlada
