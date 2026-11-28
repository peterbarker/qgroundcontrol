#pragma once

#include "QGCMapCircle.h"

/// \brief The QGCFenceCircle class provides a cicle used by GeoFence support.
///
class QGCFenceCircle : public QGCMapCircle
{
    Q_OBJECT

public:
    QGCFenceCircle(QObject* parent = nullptr);
    QGCFenceCircle(const QGeoCoordinate& center, double radius, bool inclusion, QObject* parent = nullptr);
    QGCFenceCircle(const QGCFenceCircle& other, QObject* parent = nullptr);

    const QGCFenceCircle& operator=(const QGCFenceCircle& other);

    Q_PROPERTY(bool inclusion READ inclusion WRITE setInclusion NOTIFY inclusionChanged)
    /// true: circle follows the vehicle home position (MAV_CMD_NAV_FENCE_HOME_CIRCLE_INCLUSION)
    Q_PROPERTY(bool homeCentered READ homeCentered WRITE setHomeCentered NOTIFY homeCenteredChanged)
    /// FENCE_ACTION values tried in order on breach; empty: the vehicle's configured action
    Q_PROPERTY(QList<int> breachActions READ breachActions WRITE setBreachActions NOTIFY breachActionsChanged)

    /// Sets breach action index (0: action, 1-2: fallbacks); FENCE_ACTION_DEFAULT clears it and the ones after it
    Q_INVOKABLE void setBreachAction(int index, int action);

    /// Saves the QGCFenceCircle to the json object.
    ///     @param json Json object to save to
    void saveToJson(QJsonObject& json);

    /// Load a QGCFenceCircle from json
    ///     @param json Json object to load from
    ///     @param errorString Error string if return is false
    /// @return true: success, false: failure (errorString set)
    bool loadFromJson(const QJsonObject& json, QString& errorString);

    // Property methods

    bool inclusion(void) const { return _inclusion; }

    void setInclusion(bool inclusion);

    bool homeCentered(void) const { return _homeCentered; }

    void setHomeCentered(bool homeCentered);

    QList<int> breachActions(void) const { return _breachActions; }
    void setBreachActions(const QList<int>& breachActions);

signals:
    void inclusionChanged(bool inclusion);
    void homeCenteredChanged(bool homeCentered);
    void breachActionsChanged(void);

private slots:
    void _setDirty(void);

private:
    void _init(void);

    bool _inclusion;
    bool _homeCentered = false;
    QList<int> _breachActions;

    static constexpr int _jsonCurrentVersion = 1;
    /// Home-centered circles are written as version 2 so older readers reject them rather than
    /// loading them as a fixed circle at the saved center
    static constexpr int _jsonHomeCenteredVersion = 2;
    /// Circles with breach actions are written as version 3 (with or without homeCentered) so older
    /// readers reject them rather than loading them without their actions
    static constexpr int _jsonBreachActionsVersion = 3;

    static int _jsonVersion(bool homeCentered, bool hasBreachActions);

    static constexpr const char* _jsonInclusionKey = "inclusion";
    static constexpr const char* _jsonHomeCenteredKey = "homeCentered";
};
