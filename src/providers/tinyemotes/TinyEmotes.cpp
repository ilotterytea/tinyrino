#include "providers/tinyemotes/TinyEmotes.hpp"

#include "common/Aliases.hpp"
#include "common/network/NetworkRequest.hpp"
#include "common/network/NetworkResult.hpp"
#include "common/Outcome.hpp"
#include "common/QLogging.hpp"
#include "messages/Emote.hpp"
#include "messages/Image.hpp"
#include "messages/ImageSet.hpp"
#include "messages/MessageBuilder.hpp"
#include "providers/tinyemotes/liveupdates/TinyEmotesLiveUpdateMessages.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "singletons/Settings.hpp"
#include "util/Helpers.hpp"

#include <QJsonArray>
#include <QLoggingCategory>
#include <qobject.h>
#include <QStringLiteral>
#include <QThread>

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <utility>

namespace {

using namespace chatterino;

const QString CHANNEL_HAS_NO_EMOTES("This channel has no %1 channel emotes.");

/// The emote page template.
///
/// %1 being the instance URL
/// %2 being the emote ID (e.g. 1, 123, 456789)
constexpr QStringView EMOTE_LINK_FORMAT = u"http://%1/emotes?id=%2";

/// The emote CDN link template.
///
/// %1 being the instance URL
/// %2 being the emote ID (e.g. 1, 123, 456789)
/// %3 being the emote size (e.g. 3x)
/// %4 being the emote extension (e.g. png, gif, webp)
constexpr QStringView EMOTE_CDN_FORMAT =
    u"http://%1/static/userdata/emotes/%2/%3.%4";

// TinyEmotes doesn't provide any data on the size, so we assume an emote is 32x32
constexpr QSize EMOTE_BASE_SIZE(32, 32);

struct CreateEmoteResult {
    EmoteId id;
    EmoteName name;
    Emote emote;
};

Url getEmoteLinkV3(const QString &instanceUrl, const EmoteId &id,
                   const QString &emoteScale, const QString &ext)
{
    return {EMOTE_CDN_FORMAT.arg(instanceUrl, id.string, emoteScale, ext)};
}

EmotePtr cachedOrMake(Emote &&emote, const EmoteId &id)
{
    static std::unordered_map<EmoteId, std::weak_ptr<const Emote>> cache;
    static std::mutex mutex;

    return cachedOrMakeEmotePtr(std::move(emote), cache, mutex, id);
}

std::pair<Outcome, EmoteMap> parseGlobalEmotes(const QString &instanceUrl,
                                               const QJsonObject &jsonRoot,
                                               const EmoteMap &currentEmotes)
{
    auto emotes = EmoteMap();

    if (jsonRoot.value("data").isNull())
    {
        qCInfo(chatterinoTinyemotes)
            << " No global " << instanceUrl << " emotes";
        return {Failure, std::move(emotes)};
    }

    auto jsonEmotes =
        jsonRoot.value("data").toObject().value("emotes").toArray();

    for (auto jsonEmote : jsonEmotes)
    {
        auto id =
            EmoteId{QString::number(jsonEmote.toObject().value("id").toInt())};
        auto name = EmoteName{jsonEmote.toObject().value("code").toString()};
        auto ext = jsonEmote.toObject().value("ext").toString();

        QString uploader = "anonymous*";

        if (!jsonEmote.toObject().value("uploaded_by").isNull())
        {
            uploader = jsonEmote.toObject()
                           .value("uploaded_by")
                           .toObject()
                           .value("username")
                           .toString();
        }

        auto emote = Emote({
            name,
            ImageSet{Image::fromUrl(getEmoteLinkV3(instanceUrl, id, "1x", ext),
                                    1, EMOTE_BASE_SIZE),
                     Image::fromUrl(getEmoteLinkV3(instanceUrl, id, "2x", ext),
                                    0.5, EMOTE_BASE_SIZE * 2),
                     Image::fromUrl(getEmoteLinkV3(instanceUrl, id, "3x", ext),
                                    0.25, EMOTE_BASE_SIZE * 4)},
            Tooltip{name.string + "<br>Global " + instanceUrl +
                    " Emote<br>By: " + uploader},
            Url{EMOTE_LINK_FORMAT.arg(instanceUrl, id.string)},
        });

        emotes[name] = cachedOrMakeEmotePtr(std::move(emote), currentEmotes);
    }

    return {Success, std::move(emotes)};
}

CreateEmoteResult createEmote(const QString &instanceUrl,
                              const QString &channelDisplayName,
                              const QJsonObject &jsonEmote)
{
    auto id = EmoteId{QString::number(jsonEmote.value("id").toInt())};
    auto name = EmoteName{jsonEmote.value("code").toString()};
    EmoteAuthor author;

    if (jsonEmote.value("uploaded_by").isNull())
    {
        author = EmoteAuthor{"anonymous*"};
    }
    else
    {
        author = EmoteAuthor{jsonEmote.value("uploaded_by")
                                 .toObject()
                                 .value("username")
                                 .toString()};
    }

    auto ext = jsonEmote.value("ext").toString();

    auto emote = Emote({
        name,
        ImageSet{
            Image::fromUrl(getEmoteLinkV3(instanceUrl, id, "1x", ext), 1,
                           EMOTE_BASE_SIZE),
            Image::fromUrl(getEmoteLinkV3(instanceUrl, id, "2x", ext), 0.5,
                           EMOTE_BASE_SIZE * 2),
            Image::fromUrl(getEmoteLinkV3(instanceUrl, id, "3x", ext), 0.25,
                           EMOTE_BASE_SIZE * 4),
        },
        Tooltip{QString("%1<br>Channel %2 Emote<br>By: %3")
                    .arg(name.string)
                    .arg(instanceUrl)
                    .arg(author.string)},
        Url{EMOTE_LINK_FORMAT.arg(id.string)},
        false,
        id,
    });

    return {id, name, emote};
}

bool updateChannelEmote(const QString &instanceUrl, Emote &emote,
                        const QString &channelDisplayName,
                        const QJsonObject &jsonEmote)
{
    bool anyModifications = false;

    if (jsonEmote.contains("code"))
    {
        emote.name = EmoteName{jsonEmote.value("code").toString()};
        anyModifications = true;
    }
    if (jsonEmote.contains("user"))
    {
        emote.author = EmoteAuthor{
            jsonEmote.value("user").toObject().value("displayName").toString()};
        anyModifications = true;
    }

    if (anyModifications)
    {
        emote.tooltip = Tooltip{
            QString("%1<br>%2 %4 Emote<br>By: %3")
                .arg(emote.name.string)
                // when author is empty, it is a channel emote created by the broadcaster
                .arg(emote.author.string.isEmpty() ? "Channel" : "Shared")
                .arg(emote.author.string.isEmpty() ? channelDisplayName
                                                   : emote.author.string)
                .arg(instanceUrl)};
    }

    return anyModifications;
}

}  // namespace

namespace chatterino {

using namespace tinyemotes::detail;

EmoteMap tinyemotes::detail::parseChannelEmotes(
    const QString &instanceUrl, const QJsonObject &jsonRoot,
    const QString &channelDisplayName)
{
    auto emotes = EmoteMap();

    auto jsonData = jsonRoot.value("data");
    if (jsonData.isNull())
    {
        qCInfo(chatterinoTinyemotes)
            << " No " << instanceUrl << " channel emotes for "
            << channelDisplayName;
        return emotes;
    }

    auto jsonActiveEmoteSetId =
        jsonData.toObject().value("active_emote_set_id").toInt();

    auto jsonEmoteSets = jsonData.toObject().value("emote_sets").toArray();

    auto jsonActiveEmoteSet = std::find_if(
        jsonEmoteSets.begin(), jsonEmoteSets.end(),
        [&jsonActiveEmoteSetId](const auto &it) {
            return it.toObject().value("id").toInt() == jsonActiveEmoteSetId;
        });

    if (jsonActiveEmoteSet == jsonEmoteSets.end())
    {
        return emotes;
    }

    auto jsonEmotes = jsonActiveEmoteSet->toObject().value("emotes").toArray();

    for (auto jsonEmote_ : jsonEmotes)
    {
        auto emote =
            createEmote(instanceUrl, channelDisplayName, jsonEmote_.toObject());

        emotes[emote.name] = cachedOrMake(std::move(emote.emote), emote.id);
    }

    return emotes;
}

//
// TinyEmotes
//
TinyEmotes::TinyEmotes()
{
    this->global_.insert(
        std::make_pair("localhost:8000", std::make_shared<EmoteMap>()));
    getSettings()->enableTinyGlobalEmotes.connect(
        [this] {
            this->loadEmotes();
        },
        this->managedConnections, false);
}

std::shared_ptr<const EmoteMap> TinyEmotes::emotes(
    const QString &instanceUrl) const
{
    return this->global_.at(instanceUrl).get();
}

std::optional<EmotePtr> TinyEmotes::emote(const QString &instanceUrl,
                                          const EmoteName &name) const
{
    auto emotes = this->global_.at(instanceUrl).get();
    auto it = emotes->find(name);

    if (it == emotes->end())
    {
        return std::nullopt;
    }

    return it->second;
}

void TinyEmotes::loadEmotes()
{
    for (auto it = this->global_.begin(); it != this->global_.end(); ++it)
    {
        if (!Settings::instance().enableTinyGlobalEmotes)
        {
            this->setEmotes(it->first, EMPTY_EMOTE_MAP);
            continue;
        }

        readProviderEmotesCache(
            "global", it->first, [this, &it](const auto &jsonDoc) {
                auto emotes = it->second.get();
                auto pair =
                    parseGlobalEmotes(it->first, jsonDoc.object(), *emotes);
                if (pair.first)
                {
                    this->setEmotes(it->first, std::make_shared<EmoteMap>(
                                                   std::move(pair.second)));
                }
            });

        auto url = it->first;
        auto emotes = it->second.get();

        NetworkRequest(QString(tinyemotesGlobalEmotesApiUrl).arg(it->first))
            .timeout(30000)
            .header("Accept", "application/json")
            .onSuccess([this, url, emotes](auto result) {
                writeProviderEmotesCache("global", url, result.getData());
                auto pair = parseGlobalEmotes(url, result.parseJson(), *emotes);
                if (pair.first)
                {
                    this->setEmotes(url, std::make_shared<EmoteMap>(
                                             std::move(pair.second)));
                }
            })
            .onError([&it](auto result) {
                qCWarning(chatterinoTinyemotes)
                    << "Failed to fetch global " << it->first << " emotes. "
                    << result.formatError();
            })
            .execute();
    }
}

void TinyEmotes::setEmotes(const QString &instanceUrl,
                           std::shared_ptr<const EmoteMap> emotes)
{
    auto it = this->global_.find(instanceUrl);
    if (it != this->global_.end())
    {
        it->second.set(std::move(emotes));
    }
    else
    {
        this->global_.emplace(instanceUrl, std::move(emotes));
    }
}

void TinyEmotes::loadChannel(const QString &instanceUrl,
                             std::weak_ptr<Channel> channel,
                             const QString &channelId,
                             const QString &channelDisplayName,
                             std::function<void(EmoteMap &&)> callback,
                             bool manualRefresh, bool cacheHit)
{
    NetworkRequest(
        QString(tinyemotesUserApiUrl).arg(instanceUrl).arg(channelId))
        .header("Accept", "application/json")
        .timeout(20000)
        .onSuccess([callback = std::move(callback), channel, channelId,
                    channelDisplayName, manualRefresh,
                    &instanceUrl](auto result) {
            auto emotes = parseChannelEmotes(instanceUrl, result.parseJson(),
                                             channelDisplayName);
            bool hasEmotes = !emotes.empty();
            writeProviderEmotesCache(channelId, instanceUrl, result.getData());
            callback(std::move(emotes));

            if (auto shared = channel.lock(); manualRefresh)
            {
                if (hasEmotes)
                {
                    shared->addSystemMessage(instanceUrl +
                                             " channel emotes reloaded.");
                }
                else
                {
                    shared->addSystemMessage(CHANNEL_HAS_NO_EMOTES);
                }
            }
        })
        .onError([channelId, channel, manualRefresh, cacheHit,
                  &instanceUrl](auto result) {
            auto shared = channel.lock();
            if (!shared)
            {
                return;
            }

            if (result.status() == 404)
            {
                // User does not have any Tinyemotes emotes
                if (manualRefresh)
                {
                    shared->addSystemMessage(
                        CHANNEL_HAS_NO_EMOTES.arg(instanceUrl));
                }
            }
            else
            {
                // TODO: Auto retry in case of a timeout, with a delay
                auto errorString = result.formatError();
                qCWarning(chatterinoTinyemotes)
                    << "Error fetching " << instanceUrl << " emotes for channel"
                    << channelId << ", error" << errorString;
                shared->addSystemMessage(
                    QStringLiteral("Failed to fetch %2 channel "
                                   "emotes. (Error: %1)")
                        .arg(errorString)
                        .arg(instanceUrl));
                if (cacheHit)
                {
                    shared->addSystemMessage("Using cached " + instanceUrl +
                                             " emotes as fallback.");
                }
            }
        })
        .execute();
}

EmotePtr TinyEmotes::addEmote(
    const QString &instanceUrl, const QString &channelDisplayName,
    Atomic<std::shared_ptr<const EmoteMap>> &channelEmoteMap,
    const TinyELiveUpdateEmoteUpdateAddMessage &message)
{
    // This copies the map.
    EmoteMap updatedMap = *channelEmoteMap.get();
    auto result =
        createEmote(instanceUrl, channelDisplayName, message.jsonEmote);

    auto emote = std::make_shared<const Emote>(std::move(result.emote));
    updatedMap[result.name] = emote;
    channelEmoteMap.set(std::make_shared<EmoteMap>(std::move(updatedMap)));

    return emote;
}

std::optional<std::pair<EmotePtr, EmotePtr>> TinyEmotes::updateEmote(
    const QString &instanceUrl, const QString &channelDisplayName,
    Atomic<std::shared_ptr<const EmoteMap>> &channelEmoteMap,
    const TinyELiveUpdateEmoteUpdateAddMessage &message)
{
    // This copies the map.
    EmoteMap updatedMap = *channelEmoteMap.get();

    // Step 1: remove the existing emote
    auto it = updatedMap.findEmote(QString(), message.emoteID);
    if (it == updatedMap.end())
    {
        // We already copied the map at this point and are now discarding the copy.
        // This is fine, because this case should be really rare.
        return std::nullopt;
    }
    auto oldEmotePtr = it->second;
    // copy the existing emote, to not change the original one
    auto emote = *oldEmotePtr;
    updatedMap.erase(it);

    // Step 2: update the emote
    if (!updateChannelEmote(instanceUrl, emote, channelDisplayName,
                            message.jsonEmote))
    {
        // The emote wasn't actually updated
        return std::nullopt;
    }

    auto name = emote.name;
    auto emotePtr = std::make_shared<const Emote>(std::move(emote));
    updatedMap[name] = emotePtr;
    channelEmoteMap.set(std::make_shared<EmoteMap>(std::move(updatedMap)));

    return std::make_pair(oldEmotePtr, emotePtr);
}

std::optional<EmotePtr> TinyEmotes::removeEmote(
    const QString &instanceUrl,
    std::map<QString, Atomic<std::shared_ptr<const EmoteMap>>>
        &channelTinyEmotesMap,
    const TinyELiveUpdateEmoteRemoveMessage &message)
{
    auto channelEmoteMap = channelTinyEmotesMap.find(instanceUrl);
    if (channelEmoteMap == channelTinyEmotesMap.end())
    {
        return std::nullopt;
    }

    // This copies the map.
    EmoteMap updatedMap = *channelEmoteMap->second.get();
    auto it = updatedMap.findEmote(QString(), message.emoteID);
    if (it == updatedMap.end())
    {
        // We already copied the map at this point and are now discarding the copy.
        // This is fine, because this case should be really rare.
        return std::nullopt;
    }
    auto emote = it->second;
    updatedMap.erase(it);
    channelEmoteMap->second.set(
        std::make_shared<EmoteMap>(std::move(updatedMap)));

    return emote;
}

}  // namespace chatterino
