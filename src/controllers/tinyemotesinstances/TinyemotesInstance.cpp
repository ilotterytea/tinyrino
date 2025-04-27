#include "controllers/tinyemotesinstances/TinyemotesInstance.hpp"

#include <QRegularExpression>
#include <QUrl>

namespace chatterino {

TinyemotesInstance::TinyemotesInstance(const QString &url, bool enableGlobalEmotes, bool enableChannelEmotes)
    : url_(url),
    globalEmotesEnabled_(enableGlobalEmotes),
    channelEmotesEnabled_(enableChannelEmotes)
{
}

bool TinyemotesInstance::operator==(const TinyemotesInstance &other) const
{
    return this == std::addressof(other);
}


const QString &TinyemotesInstance::getUrl() const
{
    return this->url_;
}

bool TinyemotesInstance::isGlobalEmotesEnabled() const
{
    return this->globalEmotesEnabled_;
}

bool TinyemotesInstance::isChannelEmotesEnabled() const
{
    return this->channelEmotesEnabled_;
}

}  // namespace chatterino
