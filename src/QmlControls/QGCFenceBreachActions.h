#pragma once

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonObject>
#include <QtCore/QList>
#include <QtCore/QString>

/// Breach actions of a fence area: the FENCE_ACTION values of the MAV_CMD_NAV_SET_FENCE_ACTION
/// item sent just before it, in the order the vehicle tries them. Empty means the vehicle's own
/// configured action.
class QGCFenceBreachActions
{
    Q_DECLARE_TR_FUNCTIONS(QGCFenceBreachActions)

public:
    /// Most actions one area can carry (params 1-3 of MAV_CMD_NAV_SET_FENCE_ACTION)
    static constexpr int maxActions = 3;

    /// Returns actions cut at the first FENCE_ACTION_DEFAULT (or invalid value) and to maxActions
    static QList<int> normalized(const QList<int>& actions);

    /// Returns actions with the one at index set to action, as the editors use it:
    /// FENCE_ACTION_DEFAULT removes it and all after it; an index past the end is ignored.
    static QList<int> withAction(const QList<int>& actions, int index, int action);

    /// Returns the actions carried by a MAV_CMD_NAV_SET_FENCE_ACTION item's params 1-3
    static QList<int> fromParams(double param1, double param2, double param3);

    /// Returns param n (0-2) of the MAV_CMD_NAV_SET_FENCE_ACTION item for actions
    static double param(const QList<int>& actions, int index);

    /// Display name of a FENCE_ACTION value, e.g. "RTL"
    static QString name(int action);

    /// Names of actions in the order they are tried, e.g. "RTL > Land"
    static QString describe(const QList<int>& actions);

    static void saveToJson(const QList<int>& actions, QJsonObject& json);

    /// Loads the optional actions key: an array of 1-3 FENCE_ACTION values, none of them FENCE_ACTION_DEFAULT
    ///     @return false: invalid (errorString set)
    static bool loadFromJson(const QJsonObject& json, QList<int>& actions, QString& errorString);

    static constexpr const char* jsonKey = "breachActions";
};
