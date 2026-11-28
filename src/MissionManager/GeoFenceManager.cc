#include "GeoFenceManager.h"
#include "Vehicle.h"
#include "QmlObjectListModel.h"
#include "QGCLoggingCategory.h"
#include "QGCFenceBreachActions.h"

QGC_LOGGING_CATEGORY(GeoFenceManagerLog, "PlanManager.GeoFenceManager")

GeoFenceManager::GeoFenceManager(Vehicle* vehicle)
    : PlanManager       (vehicle, MAV_MISSION_TYPE_FENCE)
{
    connect(this, &PlanManager::inProgressChanged,          this, &GeoFenceManager::inProgressChanged);
    connect(this, &PlanManager::error,                      this, &GeoFenceManager::error);
    connect(this, &PlanManager::removeAllComplete,          this, &GeoFenceManager::removeAllComplete);
    connect(this, &PlanManager::sendComplete,               this, &GeoFenceManager::_sendComplete);
    connect(this, &PlanManager::newMissionItemsAvailable,   this, &GeoFenceManager::_planManagerLoadComplete);
}

GeoFenceManager::~GeoFenceManager()
{

}

void GeoFenceManager::_appendBreachActionItem(const QList<int>& breachActions, QList<MissionItem*>& fenceItems)
{
    if (breachActions.isEmpty()) {
        return;
    }

    // Applies to the polygon or circle sent next
    MissionItem* item = new MissionItem(0, MAV_CMD_NAV_SET_FENCE_ACTION, MAV_FRAME_GLOBAL,
                                        QGCFenceBreachActions::param(breachActions, 0),
                                        QGCFenceBreachActions::param(breachActions, 1),
                                        QGCFenceBreachActions::param(breachActions, 2),
                                        0, 0, 0, 0,     // param 4-7 unused
                                        false,          // autocontinue
                                        false,          // isCurrentItem
                                        this);          // parent
    fenceItems.append(item);
}

void GeoFenceManager::sendToVehicle(const QGeoCoordinate&   breachReturn,
                                    QmlObjectListModel&     polygons,
                                    QmlObjectListModel&     circles)
{
    QList<MissionItem*> fenceItems;

    _sendPolygons.clear();
    _sendCircles.clear();

    for (int i=0; i<polygons.count(); i++) {
        _sendPolygons.append(*polygons.value<QGCFencePolygon*>(i));
    }
    for (int i=0; i<circles.count(); i++) {
        _sendCircles.append(*circles.value<QGCFenceCircle*>(i));
    }
    _breachReturnPoint = breachReturn;

    for (int i=0; i<_sendPolygons.count(); i++) {
        const QGCFencePolygon& polygon = _sendPolygons[i];

        _appendBreachActionItem(polygon.breachActions(), fenceItems);
        for (int j=0; j<polygon.count(); j++) {
            const QGeoCoordinate& vertex = polygon.path()[j].value<QGeoCoordinate>();

            MissionItem* item = new MissionItem(0,
                                                polygon.inclusion() ? MAV_CMD_NAV_FENCE_POLYGON_VERTEX_INCLUSION : MAV_CMD_NAV_FENCE_POLYGON_VERTEX_EXCLUSION,
                                                MAV_FRAME_GLOBAL,
                                                polygon.count(),    // vertex count
                                                0, 0, 0,            // param 2-4 unused
                                                vertex.latitude(),
                                                vertex.longitude(),
                                                0,                  // param 7 unused
                                                false,              // autocontinue
                                                false,              // isCurrentItem
                                                this);              // parent
            fenceItems.append(item);
        }
    }

    for (int i=0; i<_sendCircles.count(); i++) {
        QGCFenceCircle& circle = _sendCircles[i];

        MAV_CMD command = circle.inclusion() ? MAV_CMD_NAV_FENCE_CIRCLE_INCLUSION : MAV_CMD_NAV_FENCE_CIRCLE_EXCLUSION;
        double latitude = circle.center().latitude();
        double longitude = circle.center().longitude();
        if (circle.homeCentered()) {
            // The vehicle centers this circle on its own home position
            command = MAV_CMD_NAV_FENCE_HOME_CIRCLE_INCLUSION;
            latitude = 0;
            longitude = 0;
        }

        _appendBreachActionItem(circle.breachActions(), fenceItems);
        MissionItem* item = new MissionItem(0, command, MAV_FRAME_GLOBAL,
                                            circle.radius()->rawValue().toDouble(),  // param 1 radius
                                            0, 0, 0,                                 // param 2-4 unused
                                            latitude, longitude,
                                            0,                                       // param 7 unused
                                            false,                                   // autocontinue
                                            false,                                   // isCurrentItem
                                            this);                                   // parent
        fenceItems.append(item);
    }

    if (_breachReturnPoint.isValid()) {
        MissionItem* item = new MissionItem(0, MAV_CMD_NAV_FENCE_RETURN_POINT, MAV_FRAME_GLOBAL_RELATIVE_ALT, 0, 0, 0,
                                            0,      // param 1-4 unused
                                            breachReturn.latitude(), breachReturn.longitude(), breachReturn.altitude(),
                                            false,  // autocontinue
                                            false,  // isCurrentItem
                                            this);  // parent
        fenceItems.append(item);
    }

    // Plan manager takes control of MissionItems, so no need to delete
    writeMissionItems(fenceItems);
}

void GeoFenceManager::removeAll(void)
{
    _polygons.clear();
    _circles.clear();
    _breachReturnPoint = QGeoCoordinate();

    PlanManager::removeAll();
}

void GeoFenceManager::_sendComplete(bool error)
{
    if (error) {
        _polygons.clear();
        _circles.clear();
        _breachReturnPoint = QGeoCoordinate();
    } else {
        _polygons = _sendPolygons;
        _circles = _sendCircles;
    }
    _sendPolygons.clear();
    _sendCircles.clear();
    emit sendComplete(error);
}

void GeoFenceManager::_planManagerLoadComplete(bool removeAllRequested)
{
    bool loadFailed = false;

    Q_UNUSED(removeAllRequested);

    _polygons.clear();
    _circles.clear();

    MAV_CMD expectedCommand = (MAV_CMD)0;
    int expectedVertexCount = 0;
    QGCFencePolygon nextPolygon(true /* inclusion */);
    // Actions of a MAV_CMD_NAV_SET_FENCE_ACTION item, waiting for the polygon or circle after it
    QList<int> pendingBreachActions;
    bool breachActionsPending = false;
    const QList<MissionItem*>& fenceItems = missionItems();

    int i = 0;
    for (; i<fenceItems.count(); i++) {
        MissionItem* item = fenceItems[i];

        MAV_CMD command = item->command();

        if (command == MAV_CMD_NAV_FENCE_POLYGON_VERTEX_INCLUSION || command == MAV_CMD_NAV_FENCE_POLYGON_VERTEX_EXCLUSION) {
            if (nextPolygon.count() == 0) {
                // Starting a new polygon
                expectedVertexCount = item->param1();
                expectedCommand = command;
                nextPolygon.setBreachActions(pendingBreachActions);
                pendingBreachActions.clear();
                breachActionsPending = false;
            } else if (expectedVertexCount != item->param1()){
                // In the middle of a polygon, but count suddenly changed
                emit error(BadPolygonItemFormat, tr("GeoFence load: Vertex count change mid-polygon - actual:expected") + QString(" %1:%2").arg(item->param1()).arg(expectedVertexCount));
                break;
            } else if (expectedCommand != command) {
                // Command changed before last polygon was completely loaded
                emit error(BadPolygonItemFormat, tr("GeoFence load: Polygon type changed before last load complete - actual:expected") + QString(" %1:%2").arg(command).arg(expectedCommand));
                break;
            }
            nextPolygon.appendVertex(QGeoCoordinate(item->param5(), item->param6()));
            if (nextPolygon.count() == expectedVertexCount) {
                // Polygon is complete
                nextPolygon.setInclusion(command == MAV_CMD_NAV_FENCE_POLYGON_VERTEX_INCLUSION);
                _polygons.append(nextPolygon);
                nextPolygon.clear();
            }
        } else if (command == MAV_CMD_NAV_FENCE_CIRCLE_INCLUSION || command == MAV_CMD_NAV_FENCE_CIRCLE_EXCLUSION ||
                   command == MAV_CMD_NAV_FENCE_HOME_CIRCLE_INCLUSION) {
            if (nextPolygon.count() != 0) {
                // Incomplete polygon
                emit error(IncompletePolygonLoad, tr("GeoFence load: Incomplete polygon loaded"));
                break;
            }
            QGCFenceCircle circle(QGeoCoordinate(item->param5(), item->param6()), item->param1(),
                                  command != MAV_CMD_NAV_FENCE_CIRCLE_EXCLUSION /* inclusion */);
            circle.setHomeCentered(command == MAV_CMD_NAV_FENCE_HOME_CIRCLE_INCLUSION);
            circle.setBreachActions(pendingBreachActions);
            pendingBreachActions.clear();
            breachActionsPending = false;
            _circles.append(circle);
        } else if (command == MAV_CMD_NAV_SET_FENCE_ACTION) {
            if (nextPolygon.count() != 0) {
                emit error(IncompletePolygonLoad, tr("GeoFence load: Incomplete polygon loaded"));
                loadFailed = true;
                break;
            }
            if (breachActionsPending) {
                emit error(BadBreachActionItem, tr("GeoFence load: Breach action not followed by a polygon or circle"));
                loadFailed = true;
                break;
            }
            pendingBreachActions = QGCFenceBreachActions::fromParams(item->param1(), item->param2(), item->param3());
            breachActionsPending = true;
        } else if (command == MAV_CMD_NAV_FENCE_RETURN_POINT) {
            if (breachActionsPending) {
                emit error(BadBreachActionItem, tr("GeoFence load: Breach action not followed by a polygon or circle"));
                loadFailed = true;
                break;
            }
            _breachReturnPoint = QGeoCoordinate(item->param5(), item->param6(), item->param7());
        } else {
            emit error(UnsupportedCommand, tr("GeoFence load: Unsupported command %1").arg(item->command()));
            break;
        }
    }

    if (i == fenceItems.count() && breachActionsPending) {
        emit error(BadBreachActionItem, tr("GeoFence load: Breach action not followed by a polygon or circle"));
        loadFailed = true;
    }

    if (loadFailed) {
        _polygons.clear();
        _circles.clear();
        _breachReturnPoint = QGeoCoordinate();
    }

    emit loadComplete();
}

bool GeoFenceManager::supported(void) const
{
    return _vehicle->capabilityBits() & MAV_PROTOCOL_CAPABILITY_MISSION_FENCE;
}
