#include "QGCFenceBreachActions.h"

#include <cmath>

#include <QtCore/QJsonArray>
#include <QtCore/QStringList>

#include "MAVLinkLib.h"

namespace {

bool isAction(int action)
{
    return action > FENCE_ACTION_DEFAULT && action < FENCE_ACTION_ENUM_END;
}

/// Converts a param or JSON number to an action, range-checked before the cast; NaN is not an action
bool actionFromDouble(double value, int& action)
{
    if (!(value > static_cast<int>(FENCE_ACTION_DEFAULT) && value < static_cast<int>(FENCE_ACTION_ENUM_END)) ||
        value != std::floor(value)) {
        return false;
    }
    action = static_cast<int>(value);
    return true;
}

}  // namespace

QList<int> QGCFenceBreachActions::normalized(const QList<int>& actions)
{
    QList<int> result;
    for (const int action : actions) {
        if (!isAction(action) || result.count() == maxActions) {
            break;
        }
        result.append(action);
    }
    return result;
}

QList<int> QGCFenceBreachActions::withAction(const QList<int>& actions, int index, int action)
{
    QList<int> result = normalized(actions);
    if (index < 0 || index > result.count() || index >= maxActions) {
        return result;
    }
    if (!isAction(action)) {
        return result.mid(0, index);
    }
    if (index == result.count()) {
        result.append(action);
    } else {
        result[index] = action;
    }
    return result;
}

QList<int> QGCFenceBreachActions::fromParams(double param1, double param2, double param3)
{
    QList<int> actions;
    for (const double param : {param1, param2, param3}) {
        // NaN, like FENCE_ACTION_DEFAULT, ends the list
        int action = 0;
        if (!actionFromDouble(param, action)) {
            break;
        }
        actions.append(action);
    }
    return actions;
}

double QGCFenceBreachActions::param(const QList<int>& actions, int index)
{
    return index < actions.count() ? actions[index] : FENCE_ACTION_DEFAULT;
}

QString QGCFenceBreachActions::name(int action)
{
    switch (action) {
        case FENCE_ACTION_DEFAULT:
            return tr("Vehicle default");
        case FENCE_ACTION_REPORT:
            return tr("Report only");
        case FENCE_ACTION_HOLD:
            return tr("Hold");
        case FENCE_ACTION_GOTO_RETURN_POINT:
            return tr("Go to return point");
        case FENCE_ACTION_SMART_RTL:
            return tr("SmartRTL");
        case FENCE_ACTION_RTL:
            return tr("RTL");
        case FENCE_ACTION_LAND:
            return tr("Land");
        case FENCE_ACTION_DESCEND:
            return tr("Descend");
        case FENCE_ACTION_TERMINATE:
            return tr("Terminate");
        case FENCE_ACTION_CONTINGENCY:
            return tr("Contingency action");
        default:
            return tr("Action %1").arg(action);
    }
}

QString QGCFenceBreachActions::describe(const QList<int>& actions)
{
    if (actions.isEmpty()) {
        return name(FENCE_ACTION_DEFAULT);
    }
    QStringList names;
    for (const int action : actions) {
        names.append(name(action));
    }
    return names.join(QStringLiteral(" > "));
}

void QGCFenceBreachActions::saveToJson(const QList<int>& actions, QJsonObject& json)
{
    if (actions.isEmpty()) {
        return;
    }
    QJsonArray array;
    for (const int action : actions) {
        array.append(action);
    }
    json[jsonKey] = array;
}

bool QGCFenceBreachActions::loadFromJson(const QJsonObject& json, QList<int>& actions, QString& errorString)
{
    actions.clear();
    if (!json.contains(jsonKey)) {
        return true;
    }

    const QJsonValue value = json[jsonKey];
    if (!value.isArray() || value.toArray().isEmpty() || value.toArray().count() > maxActions) {
        errorString = tr("GeoFence %1 must be an array of 1 to %2 actions").arg(jsonKey).arg(maxActions);
        return false;
    }
    for (const QJsonValue entry : value.toArray()) {
        int action = 0;
        if (!entry.isDouble() || !actionFromDouble(entry.toDouble(), action)) {
            errorString = tr("GeoFence %1 holds an invalid action").arg(jsonKey);
            return false;
        }
        actions.append(action);
    }
    return true;
}
