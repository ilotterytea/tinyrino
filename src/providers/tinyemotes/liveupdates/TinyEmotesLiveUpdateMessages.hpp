#pragma once

#include <QJsonObject>

namespace chatterino {

struct TinyELiveUpdateEmoteUpdateAddMessage {
    TinyELiveUpdateEmoteUpdateAddMessage(const QJsonObject &json);

    QString channelID;

    QJsonObject jsonEmote;
    QString emoteName;
    QString emoteID;

    bool validate() const;

private:
    // true if the channel id is malformed
    // (e.g. doesn't start with "twitch:")
    bool badChannelID_;
};

struct TinyELiveUpdateEmoteRemoveMessage {
    TinyELiveUpdateEmoteRemoveMessage(const QJsonObject &json);

    QString channelID;
    QString emoteID;

    bool validate() const;

private:
    // true if the channel id is malformed
    // (e.g. doesn't start with "twitch:")
    bool badChannelID_;
};

}  // namespace chatterino
