#include "QGCFenceCircle.h"
#include "JsonParsing.h"
#include "QGCFenceBreachActions.h"

QGCFenceCircle::QGCFenceCircle(QObject* parent)
    : QGCMapCircle  (parent)
    , _inclusion    (true)
{
    _init();
}

QGCFenceCircle::QGCFenceCircle(const QGeoCoordinate& center, double radius, bool inclusion, QObject* parent)
    : QGCMapCircle  (center, radius, false /* showRotation */, true /* clockwiseRotation */, parent)
    , _inclusion    (inclusion)
{
    _init();
}

QGCFenceCircle::QGCFenceCircle(const QGCFenceCircle& other, QObject* parent)
    : QGCMapCircle(other, parent)
    , _inclusion(other._inclusion)
    , _homeCentered(other._homeCentered)
    , _breachActions(other._breachActions)
{
    _init();
}

void QGCFenceCircle::_init(void)
{
    connect(this, &QGCFenceCircle::inclusionChanged, this, &QGCFenceCircle::_setDirty);
    connect(this, &QGCFenceCircle::homeCenteredChanged, this, &QGCFenceCircle::_setDirty);
    connect(this, &QGCFenceCircle::breachActionsChanged, this, &QGCFenceCircle::_setDirty);
}

const QGCFenceCircle& QGCFenceCircle::operator=(const QGCFenceCircle& other)
{
    QGCMapCircle::operator=(other);

    setHomeCentered(other._homeCentered);
    setInclusion(other._inclusion);
    setBreachActions(other._breachActions);

    return *this;
}

void QGCFenceCircle::_setDirty(void)
{
    setDirty(true);
}

void QGCFenceCircle::saveToJson(QJsonObject& json)
{
    json[JsonParsing::jsonVersionKey] = _jsonVersion(_homeCentered, !_breachActions.isEmpty());
    json[_jsonInclusionKey] = _inclusion;
    if (_homeCentered) {
        json[_jsonHomeCenteredKey] = true;
    }
    QGCFenceBreachActions::saveToJson(_breachActions, json);
    QGCMapCircle::saveToJson(json);
}

bool QGCFenceCircle::loadFromJson(const QJsonObject& json, QString& errorString)
{
    errorString.clear();

    QList<JsonParsing::KeyValidateInfo> keyInfoList = {
        {JsonParsing::jsonVersionKey, QJsonValue::Double, true},
        {_jsonInclusionKey, QJsonValue::Bool, true},
        {_jsonHomeCenteredKey, QJsonValue::Bool, false},
        {QGCFenceBreachActions::jsonKey, QJsonValue::Array, false},
    };
    if (!JsonParsing::validateKeys(json, keyInfoList, errorString)) {
        return false;
    }

    const int version = json[JsonParsing::jsonVersionKey].toInt();
    if (version != _jsonCurrentVersion && version != _jsonHomeCenteredVersion && version != _jsonBreachActionsVersion) {
        errorString = tr("GeoFence Circle only supports versions %1 to %2")
                          .arg(_jsonCurrentVersion)
                          .arg(_jsonBreachActionsVersion);
        return false;
    }

    QList<int> breachActions;
    if (!QGCFenceBreachActions::loadFromJson(json, breachActions, errorString)) {
        return false;
    }

    const bool homeCentered = json[_jsonHomeCenteredKey].toBool(false);
    if (version != _jsonVersion(homeCentered, !breachActions.isEmpty())) {
        errorString = tr("GeoFence Circle version %1 does not match its home-centered setting or breach actions").arg(version);
        return false;
    }

    if (!QGCMapCircle::loadFromJson(json, errorString)) {
        return false;
    }

    setHomeCentered(homeCentered);
    setInclusion(json[_jsonInclusionKey].toBool());
    setBreachActions(breachActions);

    return true;
}

void QGCFenceCircle::setInclusion(bool inclusion)
{
    if (_homeCentered && !inclusion) {
        // Home circles are inclusion-only
        return;
    }

    if (inclusion != _inclusion) {
        _inclusion = inclusion;
        emit inclusionChanged(inclusion);
    }
}

void QGCFenceCircle::setHomeCentered(bool homeCentered)
{
    if (homeCentered != _homeCentered) {
        _homeCentered = homeCentered;
        if (_homeCentered) {
            // Home circles are inclusion-only
            setInclusion(true);
        }
        emit homeCenteredChanged(homeCentered);
    }
}

void QGCFenceCircle::setBreachActions(const QList<int>& breachActions)
{
    const QList<int> actions = QGCFenceBreachActions::normalized(breachActions);
    if (actions != _breachActions) {
        _breachActions = actions;
        emit breachActionsChanged();
    }
}

void QGCFenceCircle::setBreachAction(int index, int action)
{
    setBreachActions(QGCFenceBreachActions::withAction(_breachActions, index, action));
}

int QGCFenceCircle::_jsonVersion(bool homeCentered, bool hasBreachActions)
{
    if (hasBreachActions) {
        return _jsonBreachActionsVersion;
    }
    return homeCentered ? _jsonHomeCenteredVersion : _jsonCurrentVersion;
}
