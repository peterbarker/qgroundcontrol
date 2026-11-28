#pragma once

#include "QGCMapPolygon.h"

/// \brief The QGCFencePolygon class provides a polygon used by GeoFence support.
///
class QGCFencePolygon : public QGCMapPolygon
{
    Q_OBJECT

public:
    QGCFencePolygon(bool inclusion, QObject* parent = nullptr);
    QGCFencePolygon(const QGCFencePolygon& other, QObject* parent = nullptr);

    const QGCFencePolygon& operator=(const QGCFencePolygon& other);

    Q_PROPERTY(bool inclusion READ inclusion WRITE setInclusion NOTIFY inclusionChanged)
    /// FENCE_ACTION values tried in order on breach; empty: the vehicle's configured action
    Q_PROPERTY(QList<int> breachActions READ breachActions WRITE setBreachActions NOTIFY breachActionsChanged)

    /// Sets breach action index (0: action, 1-2: fallbacks); FENCE_ACTION_DEFAULT clears it and the ones after it
    Q_INVOKABLE void setBreachAction(int index, int action);

    /// Saves the QGCFencePolygon to the json object.
    ///     @param json Json object to save to
    void saveToJson(QJsonObject& json);

    /// Load a QGCFencePolygon from json
    ///     @param json Json object to load from
    ///     @param required true: no polygon in object will generate error
    ///     @param errorString Error string if return is false
    /// @return true: success, false: failure (errorString set)
    bool loadFromJson(const QJsonObject& json, bool required, QString& errorString);

    // Property methods

    bool inclusion      (void) const { return _inclusion; }
    void setInclusion   (bool inclusion);

    QList<int> breachActions(void) const { return _breachActions; }
    void setBreachActions(const QList<int>& breachActions);

signals:
    void inclusionChanged   (bool inclusion);
    void breachActionsChanged(void);

private slots:
    void _setDirty(void);

private:
    void _init(void);

    bool _inclusion;
    QList<int> _breachActions;

    static constexpr int _jsonCurrentVersion = 1;
    /// Polygons with breach actions are written as version 2 so older readers reject them rather than
    /// loading them without their actions
    static constexpr int _jsonBreachActionsVersion = 2;

    static constexpr const char* _jsonInclusionKey = "inclusion";
};
