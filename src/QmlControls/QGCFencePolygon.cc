#include "QGCFencePolygon.h"
#include "JsonParsing.h"
#include "QGCFenceBreachActions.h"

QGCFencePolygon::QGCFencePolygon(bool inclusion, QObject* parent)
    : QGCMapPolygon (parent)
    , _inclusion    (inclusion)
{
    _init();
}

QGCFencePolygon::QGCFencePolygon(const QGCFencePolygon& other, QObject* parent)
    : QGCMapPolygon (other, parent)
    , _inclusion    (other._inclusion)
    , _breachActions(other._breachActions)
{
    _init();
}

void QGCFencePolygon::_init(void)
{
    connect(this, &QGCFencePolygon::inclusionChanged, this, &QGCFencePolygon::_setDirty);
    connect(this, &QGCFencePolygon::breachActionsChanged, this, &QGCFencePolygon::_setDirty);
}

const QGCFencePolygon& QGCFencePolygon::operator=(const QGCFencePolygon& other)
{
    QGCMapPolygon::operator=(other);

    setInclusion(other._inclusion);
    setBreachActions(other._breachActions);

    return *this;
}

void QGCFencePolygon::_setDirty(void)
{
    setDirty(true);
}

void QGCFencePolygon::saveToJson(QJsonObject& json)
{
    json[JsonParsing::jsonVersionKey] = _breachActions.isEmpty() ? _jsonCurrentVersion : _jsonBreachActionsVersion;
    json[_jsonInclusionKey] = _inclusion;
    QGCFenceBreachActions::saveToJson(_breachActions, json);
    QGCMapPolygon::saveToJson(json);
}

bool QGCFencePolygon::loadFromJson(const QJsonObject& json, bool required, QString& errorString)
{
    errorString.clear();

    QList<JsonParsing::KeyValidateInfo> keyInfoList = {
        { JsonParsing::jsonVersionKey,   QJsonValue::Double, true },
        { _jsonInclusionKey,            QJsonValue::Bool,   true },
        { QGCFenceBreachActions::jsonKey, QJsonValue::Array, false },
    };
    if (!JsonParsing::validateKeys(json, keyInfoList, errorString)) {
        return false;
    }

    const int version = json[JsonParsing::jsonVersionKey].toInt();
    if (version != _jsonCurrentVersion && version != _jsonBreachActionsVersion) {
        errorString = tr("GeoFence Polygon only supports versions %1 and %2")
                          .arg(_jsonCurrentVersion)
                          .arg(_jsonBreachActionsVersion);
        return false;
    }

    QList<int> breachActions;
    if (!QGCFenceBreachActions::loadFromJson(json, breachActions, errorString)) {
        return false;
    }
    if (breachActions.isEmpty() != (version == _jsonCurrentVersion)) {
        errorString = tr("GeoFence Polygon version %1 does not match its breach actions").arg(version);
        return false;
    }

    if (!QGCMapPolygon::loadFromJson(json, required, errorString)) {
        return false;
    }

    setInclusion(json[_jsonInclusionKey].toBool());
    setBreachActions(breachActions);

    return true;
}

void QGCFencePolygon::setInclusion(bool inclusion)
{
    if (inclusion != _inclusion) {
        _inclusion = inclusion;
        emit inclusionChanged(inclusion);
    }
}

void QGCFencePolygon::setBreachActions(const QList<int>& breachActions)
{
    const QList<int> actions = QGCFenceBreachActions::normalized(breachActions);
    if (actions != _breachActions) {
        _breachActions = actions;
        emit breachActionsChanged();
    }
}

void QGCFencePolygon::setBreachAction(int index, int action)
{
    setBreachActions(QGCFenceBreachActions::withAction(_breachActions, index, action));
}
