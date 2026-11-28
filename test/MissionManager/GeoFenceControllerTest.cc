#include "GeoFenceControllerTest.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
#include <QtCore/QTemporaryDir>
#include <QtTest/QSignalSpy>

#include "CoordFixtures.h"
#include "FirmwarePlugin.h"
#include "GeoFenceController.h"
#include "GeoFenceManager.h"
#include "JsonParsing.h"
#include "MissionController.h"
#include "MissionItem.h"
#include "PlanMasterController.h"
#include "QGCFenceCircle.h"
#include "QGCFencePolygon.h"
#include "QmlObjectListModel.h"
#include "UnitTestCoords.h"
#include "Vehicle.h"

namespace {

QGeoCoordinate home2D()
{
    const QGeoCoordinate home = TestFixtures::Coord::zurich();
    return QGeoCoordinate(home.latitude(), home.longitude());
}

/// Adds a circle away from home and returns it
QGCFenceCircle* addCircle(GeoFenceController* controller)
{
    const QGeoCoordinate center = home2D().atDistanceAndAzimuth(1000, 90);
    controller->addInclusionCircle(center.atDistanceAndAzimuth(300, 315), center.atDistanceAndAzimuth(300, 135));
    return controller->circles()->value<QGCFenceCircle*>(controller->circles()->count() - 1);
}

/// Adds a polygon away from home and returns it
QGCFencePolygon* addPolygon(GeoFenceController* controller)
{
    const QGeoCoordinate center = home2D().atDistanceAndAzimuth(1000, 270);
    controller->addInclusionPolygon(center.atDistanceAndAzimuth(300, 315), center.atDistanceAndAzimuth(300, 135));
    return controller->polygons()->value<QGCFencePolygon*>(controller->polygons()->count() - 1);
}

/// Breach actions ArduCopter accepts (MockLink's default vehicle is a quadrotor)
const QList<int> copterBreachActions = {FENCE_ACTION_REPORT, FENCE_ACTION_HOLD, FENCE_ACTION_SMART_RTL,
                                        FENCE_ACTION_RTL,    FENCE_ACTION_LAND, FENCE_ACTION_DESCEND};

}  // namespace

GeoFenceControllerTest::GeoFenceControllerTest(QObject* parent)
    : MissionTest(parent)
{
    setAutopilotType(MAV_AUTOPILOT_ARDUPILOTMEGA);
}

void GeoFenceControllerTest::_testHomeCircleSupportedVersion_data()
{
    QTest::addColumn<int>("major");
    QTest::addColumn<int>("minor");
    QTest::addColumn<int>("patch");
    QTest::addColumn<bool>("supported");

    QTest::newRow("4.7.9") << 4 << 7 << 9 << false;
    QTest::newRow("4.8.0") << 4 << 8 << 0 << true;
    QTest::newRow("5.0.0") << 5 << 0 << 0 << true;
}

void GeoFenceControllerTest::_testHomeCircleSupportedVersion()
{
    QFETCH(int, major);
    QFETCH(int, minor);
    QFETCH(int, patch);
    QFETCH(bool, supported);

    QSignalSpy spy(geoFenceController(), &GeoFenceController::homeCircleSupportedChanged);
    vehicle()->setFirmwareVersion(major, minor, patch);
    QVERIFY(spy.count() >= 1);
    QCOMPARE(geoFenceController()->homeCircleSupported(), supported);
    QVERIFY(!vehicle()->firmwarePlugin()->supportsFenceHomeCircle(nullptr));
}

void GeoFenceControllerTest::_testHomeCircleVehicleRoundTrip()
{
    GeoFenceController* const controller = geoFenceController();
    missionController()->setHomePosition(TestFixtures::Coord::zurich());

    QGCFenceCircle* circle = addCircle(controller);
    QVERIFY(circle);
    circle->setInclusion(false);
    circle->radius()->setRawValue(75.0);
    circle->setHomeCentered(true);
    QVERIFY(circle->inclusion());
    circle->setInclusion(false);
    QVERIFY(circle->inclusion());
    QCOMPARE_COORDS(circle->center(), home2D());

    GeoFenceManager* const manager = vehicle()->geoFenceManager();
    QSignalSpy sendSpy(manager, &GeoFenceManager::sendComplete);
    controller->sendToVehicle();
    QTRY_COMPARE_WITH_TIMEOUT(sendSpy.count(), 1, TestTimeout::mediumMs());
    QCOMPARE(sendSpy.takeFirst().at(0).toBool(), false);

    QSignalSpy loadSpy(controller, &GeoFenceController::loadComplete);
    controller->loadFromVehicle();
    QTRY_COMPARE_WITH_TIMEOUT(loadSpy.count(), 1, TestTimeout::mediumMs());

    QCOMPARE(manager->missionItems().count(), 1);
    const MissionItem* item = manager->missionItems().first();
    QCOMPARE(item->command(), MAV_CMD_NAV_FENCE_HOME_CIRCLE_INCLUSION);
    QCOMPARE(item->param1(), 75.0);
    QCOMPARE(item->param5(), 0.0);
    QCOMPARE(item->param6(), 0.0);

    QCOMPARE(controller->circles()->count(), 1);
    circle = controller->circles()->value<QGCFenceCircle*>(0);
    QVERIFY(circle->homeCentered());
    QVERIFY(circle->inclusion());
    QCOMPARE(circle->radius()->rawValue().toDouble(), 75.0);
    QCOMPARE_COORDS(circle->center(), home2D());
    QVERIFY(!controller->dirty());
}

void GeoFenceControllerTest::_testHomeCircleFollowsHomeWithoutDirty()
{
    vehicle()->setFirmwareVersion(4, 8, 0);
    missionController()->setHomePosition(TestFixtures::Coord::zurich());
    GeoFenceController* const controller = geoFenceController();
    addCircle(controller)->setHomeCentered(true);
    addCircle(controller);

    // Start from a plan that is clean for both upload and save
    planController()->sendToVehicle();
    QTRY_VERIFY_WITH_TIMEOUT(!planController()->syncInProgress() && !planController()->dirtyForUpload(),
                             TestTimeout::mediumMs());
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QVERIFY(planController()->saveToFile(tempDir.filePath(QStringLiteral("home.plan"))));
    QVERIFY(!planController()->dirtyForSave());

    // Fly view reloaded the fence from the vehicle after the upload
    QCOMPARE(controller->circles()->count(), 2);
    QGCFenceCircle* const homeCircle = controller->circles()->value<QGCFenceCircle*>(0);
    QGCFenceCircle* const fixedCircle = controller->circles()->value<QGCFenceCircle*>(1);
    QVERIFY(homeCircle->homeCentered());
    const QGeoCoordinate fixedCenter = fixedCircle->center();

    // PlanMasterController latches the plan dirty on any of these, even if later cleared
    QSignalSpy dirtySpy(controller, &GeoFenceController::dirtyChanged);
    const QGeoCoordinate newHome = TestFixtures::Coord::zurich().atDistanceAndAzimuth(500, 0);
    Vehicle* const flyVehicle = vehicle();
    emit flyVehicle->homePositionChanged(newHome);

    QCOMPARE_COORDS(homeCircle->center(), newHome);
    QCOMPARE(fixedCircle->center(), fixedCenter);
    QCOMPARE(dirtySpy.count(), 0);
    QVERIFY(!controller->dirty());
    QVERIFY(!planController()->dirtyForSave());
    QVERIFY(!planController()->dirtyForUpload());
}

void GeoFenceControllerTest::_testUploadRefusedWhenHomeCircleUnsupported()
{
    vehicle()->setFirmwareVersion(4, 7, 0);
    GeoFenceController* const controller = geoFenceController();
    QVERIFY(!controller->homeCircleSupported());

    QSignalSpy hasHomeSpy(controller, &GeoFenceController::hasHomeCircleChanged);
    QGCFenceCircle* const circle = addCircle(controller);
    QVERIFY(!controller->hasHomeCircle());
    circle->setHomeCentered(true);
    QVERIFY(controller->hasHomeCircle());
    QVERIFY(hasHomeSpy.count() >= 2);

    expectAppMessage(QRegularExpression(QStringLiteral("does not support home-centered")));
    planController()->sendToVehicle();
    verifyExpectedLogMessage();
    QVERIFY(!planController()->syncInProgress());
    QVERIFY(vehicle()->geoFenceManager()->missionItems().isEmpty());

    circle->setHomeCentered(false);
    QVERIFY(!controller->hasHomeCircle());
}

void GeoFenceControllerTest::_testHomeCircleJsonRoundTrip()
{
    GeoFenceController* const controller = geoFenceController();
    missionController()->setHomePosition(TestFixtures::Coord::zurich());

    addCircle(controller)->setHomeCentered(true);
    addCircle(controller);

    QJsonObject json;
    controller->save(json);

    // Home circles use a newer version so older QGC rejects them; ordinary circles stay readable
    const QJsonArray jsonCircles = json[QStringLiteral("circles")].toArray();
    QCOMPARE(jsonCircles.count(), 2);
    QCOMPARE(jsonCircles[0].toObject()[JsonParsing::jsonVersionKey].toInt(), 2);
    QCOMPARE(jsonCircles[1].toObject()[JsonParsing::jsonVersionKey].toInt(), 1);
    QVERIFY(!jsonCircles[1].toObject().contains(QStringLiteral("homeCentered")));

    const QGeoCoordinate newHome = TestFixtures::Coord::zurich().atDistanceAndAzimuth(500, 0);
    missionController()->setHomePosition(newHome);

    QString errorString;
    QVERIFY2(controller->load(json, errorString), qPrintable(errorString));
    QCOMPARE(controller->circles()->count(), 2);

    const QGCFenceCircle* const homeCircle = controller->circles()->value<QGCFenceCircle*>(0);
    QVERIFY(homeCircle->homeCentered());
    QCOMPARE_COORDS(homeCircle->center(), newHome);
    QVERIFY(!controller->circles()->value<QGCFenceCircle*>(1)->homeCentered());
    QVERIFY(!controller->dirty());
}

void GeoFenceControllerTest::_testCircleJsonVersionMatchesHomeCentered_data()
{
    QTest::addColumn<int>("version");
    QTest::addColumn<bool>("hasKey");
    QTest::addColumn<bool>("keyValue");
    QTest::addColumn<bool>("loads");

    QTest::newRow("v1 without key") << 1 << false << false << true;
    QTest::newRow("v1 key false") << 1 << true << false << true;
    QTest::newRow("v2 key true") << 2 << true << true << true;
    QTest::newRow("v1 key true") << 1 << true << true << false;
    QTest::newRow("v2 without key") << 2 << false << false << false;
    QTest::newRow("v2 key false") << 2 << true << false << false;
}

void GeoFenceControllerTest::_testCircleJsonVersionMatchesHomeCentered()
{
    QFETCH(int, version);
    QFETCH(bool, hasKey);
    QFETCH(bool, keyValue);
    QFETCH(bool, loads);

    QJsonObject json;
    QGCFenceCircle(home2D(), 100.0, false /* inclusion */).saveToJson(json);
    json[JsonParsing::jsonVersionKey] = version;
    if (hasKey) {
        json[QStringLiteral("homeCentered")] = keyValue;
    }

    // Starts as a home circle so loading must also be able to turn that off
    QGCFenceCircle circle;
    circle.setHomeCentered(true);
    QString errorString;
    QCOMPARE(circle.loadFromJson(json, errorString), loads);
    if (loads) {
        QCOMPARE(circle.homeCentered(), keyValue);
        QCOMPARE(circle.inclusion(), keyValue);
    } else {
        QVERIFY(!errorString.isEmpty());
    }
}

void GeoFenceControllerTest::_testBreachActionsSupportedVersion_data()
{
    QTest::addColumn<int>("major");
    QTest::addColumn<int>("minor");
    QTest::addColumn<bool>("supported");

    QTest::newRow("4.7") << 4 << 7 << false;
    QTest::newRow("4.8") << 4 << 8 << true;
}

void GeoFenceControllerTest::_testBreachActionsSupportedVersion()
{
    QFETCH(int, major);
    QFETCH(int, minor);
    QFETCH(bool, supported);

    GeoFenceController* const controller = geoFenceController();
    QSignalSpy spy(controller, &GeoFenceController::breachActionsSupportedChanged);
    vehicle()->setFirmwareVersion(major, minor, 0);
    QVERIFY(spy.count() >= 1);
    QCOMPARE(controller->breachActionsSupported(), supported);
    QVERIFY(vehicle()->firmwarePlugin()->supportedFenceBreachActions(nullptr).isEmpty());

    QList<int> choices;
    for (const QVariant& choice : controller->breachActionChoices()) {
        const QVariantMap map = choice.toMap();
        QVERIFY(!map.value(QStringLiteral("text")).toString().isEmpty());
        choices.append(map.value(QStringLiteral("value")).toInt());
    }
    QCOMPARE(choices, supported ? copterBreachActions : QList<int>());
}

void GeoFenceControllerTest::_testSetBreachAction()
{
    QGCFencePolygon* const polygon = addPolygon(geoFenceController());
    QSignalSpy hasSpy(geoFenceController(), &GeoFenceController::hasBreachActionsChanged);

    // A fallback needs the action before it
    polygon->setBreachAction(1, FENCE_ACTION_LAND);
    QVERIFY(polygon->breachActions().isEmpty());
    QVERIFY(!geoFenceController()->hasBreachActions());

    polygon->setBreachAction(0, FENCE_ACTION_RTL);
    polygon->setBreachAction(1, FENCE_ACTION_LAND);
    polygon->setBreachAction(2, FENCE_ACTION_DESCEND);
    polygon->setBreachAction(3, FENCE_ACTION_REPORT);
    QCOMPARE(polygon->breachActions(), QList<int>({FENCE_ACTION_RTL, FENCE_ACTION_LAND, FENCE_ACTION_DESCEND}));
    QVERIFY(geoFenceController()->hasBreachActions());
    QVERIFY(hasSpy.count() >= 1);
    QCOMPARE(geoFenceController()->breachActionsDescription(polygon->breachActions()),
             QStringLiteral("RTL > Land > Descend"));

    // Replacing keeps the fallbacks; clearing a level drops the ones after it
    polygon->setBreachAction(0, FENCE_ACTION_SMART_RTL);
    QCOMPARE(polygon->breachActions(), QList<int>({FENCE_ACTION_SMART_RTL, FENCE_ACTION_LAND, FENCE_ACTION_DESCEND}));
    polygon->setBreachAction(1, FENCE_ACTION_DEFAULT);
    QCOMPARE(polygon->breachActions(), QList<int>({FENCE_ACTION_SMART_RTL}));
    polygon->setBreachAction(0, FENCE_ACTION_DEFAULT);
    QVERIFY(polygon->breachActions().isEmpty());
    QVERIFY(!geoFenceController()->hasBreachActions());

    // Lists are cut at the first default and to three actions
    polygon->setBreachActions({FENCE_ACTION_RTL, FENCE_ACTION_DEFAULT, FENCE_ACTION_LAND});
    QCOMPARE(polygon->breachActions(), QList<int>({FENCE_ACTION_RTL}));
    polygon->setBreachActions({FENCE_ACTION_HOLD, FENCE_ACTION_RTL, FENCE_ACTION_LAND, FENCE_ACTION_DESCEND});
    QCOMPARE(polygon->breachActions(), QList<int>({FENCE_ACTION_HOLD, FENCE_ACTION_RTL, FENCE_ACTION_LAND}));
}

void GeoFenceControllerTest::_testBreachActionsVehicleRoundTrip()
{
    GeoFenceController* const controller = geoFenceController();
    missionController()->setHomePosition(TestFixtures::Coord::zurich());

    QGCFencePolygon* const polygon = addPolygon(controller);
    polygon->setBreachActions({FENCE_ACTION_RTL, FENCE_ACTION_LAND});
    const int vertexCount = polygon->count();
    addCircle(controller)->setBreachActions({FENCE_ACTION_HOLD});
    addCircle(controller);

    GeoFenceManager* const manager = vehicle()->geoFenceManager();
    QSignalSpy sendSpy(manager, &GeoFenceManager::sendComplete);
    controller->sendToVehicle();
    QTRY_COMPARE_WITH_TIMEOUT(sendSpy.count(), 1, TestTimeout::mediumMs());
    QCOMPARE(sendSpy.takeFirst().at(0).toBool(), false);

    QSignalSpy loadSpy(controller, &GeoFenceController::loadComplete);
    controller->loadFromVehicle();
    QTRY_COMPARE_WITH_TIMEOUT(loadSpy.count(), 1, TestTimeout::mediumMs());

    // Each action item sits just before the fence it applies to
    const QList<MissionItem*>& items = manager->missionItems();
    QCOMPARE(items.count(), 1 + vertexCount + 1 + 1 + 1);
    QCOMPARE(items[0]->command(), MAV_CMD_NAV_SET_FENCE_ACTION);
    QCOMPARE(items[0]->frame(), MAV_FRAME_GLOBAL);
    QCOMPARE(items[0]->param1(), static_cast<double>(FENCE_ACTION_RTL));
    QCOMPARE(items[0]->param2(), static_cast<double>(FENCE_ACTION_LAND));
    QCOMPARE(items[0]->param3(), static_cast<double>(FENCE_ACTION_DEFAULT));
    QCOMPARE(items[1]->command(), MAV_CMD_NAV_FENCE_POLYGON_VERTEX_INCLUSION);
    const int circleAction = 1 + vertexCount;
    QCOMPARE(items[circleAction]->command(), MAV_CMD_NAV_SET_FENCE_ACTION);
    QCOMPARE(items[circleAction]->param1(), static_cast<double>(FENCE_ACTION_HOLD));
    QCOMPARE(items[circleAction]->param2(), static_cast<double>(FENCE_ACTION_DEFAULT));
    QCOMPARE(items[circleAction + 1]->command(), MAV_CMD_NAV_FENCE_CIRCLE_INCLUSION);
    QCOMPARE(items[circleAction + 2]->command(), MAV_CMD_NAV_FENCE_CIRCLE_INCLUSION);

    QCOMPARE(controller->polygons()->count(), 1);
    QCOMPARE(controller->polygons()->value<QGCFencePolygon*>(0)->count(), vertexCount);
    QCOMPARE(controller->polygons()->value<QGCFencePolygon*>(0)->breachActions(),
             QList<int>({FENCE_ACTION_RTL, FENCE_ACTION_LAND}));
    QCOMPARE(controller->circles()->count(), 2);
    QCOMPARE(controller->circles()->value<QGCFenceCircle*>(0)->breachActions(), QList<int>({FENCE_ACTION_HOLD}));
    QVERIFY(controller->circles()->value<QGCFenceCircle*>(1)->breachActions().isEmpty());
    QVERIFY(!controller->dirty());
}

void GeoFenceControllerTest::_testUploadRefusedWhenBreachActionUnsupported()
{
    GeoFenceController* const controller = geoFenceController();
    QGCFencePolygon* const polygon = addPolygon(controller);

    // Copter has no terminate action
    vehicle()->setFirmwareVersion(4, 8, 0);
    polygon->setBreachActions({FENCE_ACTION_RTL, FENCE_ACTION_TERMINATE});
    QVERIFY(!controller->breachActionsUploadable());
    expectAppMessage(QRegularExpression(QStringLiteral("does not support some of the breach actions")));
    planController()->sendToVehicle();
    verifyExpectedLogMessage();
    QVERIFY(!planController()->syncInProgress());
    QVERIFY(vehicle()->geoFenceManager()->missionItems().isEmpty());

    polygon->setBreachActions({FENCE_ACTION_RTL});
    QVERIFY(controller->breachActionsUploadable());

    // Older firmware supports none
    vehicle()->setFirmwareVersion(4, 7, 0);
    QVERIFY(!controller->breachActionsUploadable());
    polygon->setBreachActions({});
    QVERIFY(controller->breachActionsUploadable());
}

void GeoFenceControllerTest::_testBreachActionsJsonRoundTrip()
{
    GeoFenceController* const controller = geoFenceController();
    missionController()->setHomePosition(TestFixtures::Coord::zurich());

    addPolygon(controller)->setBreachActions({FENCE_ACTION_RTL, FENCE_ACTION_LAND});
    addPolygon(controller);
    QGCFenceCircle* const homeCircle = addCircle(controller);
    homeCircle->setHomeCentered(true);
    homeCircle->setBreachActions({FENCE_ACTION_HOLD});
    addCircle(controller);

    QJsonObject json;
    controller->save(json);

    // Fences with actions use a newer version so older QGC rejects them; the others stay readable
    const QJsonArray jsonPolygons = json[QStringLiteral("polygons")].toArray();
    QCOMPARE(jsonPolygons.count(), 2);
    QCOMPARE(jsonPolygons[0].toObject()[JsonParsing::jsonVersionKey].toInt(), 2);
    QCOMPARE(jsonPolygons[0].toObject()[QStringLiteral("breachActions")].toArray(), QJsonArray({5, 6}));
    QCOMPARE(jsonPolygons[1].toObject()[JsonParsing::jsonVersionKey].toInt(), 1);
    QVERIFY(!jsonPolygons[1].toObject().contains(QStringLiteral("breachActions")));
    const QJsonArray jsonCircles = json[QStringLiteral("circles")].toArray();
    QCOMPARE(jsonCircles.count(), 2);
    QCOMPARE(jsonCircles[0].toObject()[JsonParsing::jsonVersionKey].toInt(), 3);
    QCOMPARE(jsonCircles[1].toObject()[JsonParsing::jsonVersionKey].toInt(), 1);

    QString errorString;
    QVERIFY2(controller->load(json, errorString), qPrintable(errorString));
    QCOMPARE(controller->polygons()->count(), 2);
    QCOMPARE(controller->polygons()->value<QGCFencePolygon*>(0)->breachActions(),
             QList<int>({FENCE_ACTION_RTL, FENCE_ACTION_LAND}));
    QVERIFY(controller->polygons()->value<QGCFencePolygon*>(1)->breachActions().isEmpty());
    QCOMPARE(controller->circles()->count(), 2);
    QVERIFY(controller->circles()->value<QGCFenceCircle*>(0)->homeCentered());
    QCOMPARE(controller->circles()->value<QGCFenceCircle*>(0)->breachActions(), QList<int>({FENCE_ACTION_HOLD}));
    QVERIFY(controller->circles()->value<QGCFenceCircle*>(1)->breachActions().isEmpty());
    QVERIFY(controller->hasBreachActions());
    QVERIFY(!controller->dirty());
}

void GeoFenceControllerTest::_testFenceJsonVersionMatchesBreachActions_data()
{
    QTest::addColumn<bool>("circle");
    QTest::addColumn<int>("version");
    QTest::addColumn<bool>("homeCentered");
    QTest::addColumn<QJsonValue>("actions");
    QTest::addColumn<bool>("loads");

    const QJsonValue none;  // key absent
    const QJsonValue rtlLand(QJsonArray({5, 6}));

    QTest::newRow("polygon v1") << false << 1 << false << none << true;
    QTest::newRow("polygon v2 actions") << false << 2 << false << rtlLand << true;
    QTest::newRow("polygon v1 actions") << false << 1 << false << rtlLand << false;
    QTest::newRow("polygon v2 no actions") << false << 2 << false << none << false;
    QTest::newRow("circle v3 actions") << true << 3 << false << rtlLand << true;
    QTest::newRow("circle v3 home actions") << true << 3 << true << rtlLand << true;
    QTest::newRow("circle v2 home actions") << true << 2 << true << rtlLand << false;
    QTest::newRow("circle v1 actions") << true << 1 << false << rtlLand << false;
    QTest::newRow("circle v3 no actions") << true << 3 << false << none << false;
    QTest::newRow("empty list") << false << 2 << false << QJsonValue(QJsonArray()) << false;
    QTest::newRow("default in list") << false << 2 << false << QJsonValue(QJsonArray({5, 0})) << false;
    QTest::newRow("four actions") << false << 2 << false << QJsonValue(QJsonArray({2, 5, 6, 7})) << false;
    QTest::newRow("unknown action") << false << 2 << false << QJsonValue(QJsonArray({99})) << false;
    QTest::newRow("huge action") << false << 2 << false << QJsonValue(QJsonArray({1e20})) << false;
    QTest::newRow("fractional action") << false << 2 << false << QJsonValue(QJsonArray({5.5})) << false;
    QTest::newRow("not a list") << false << 2 << false << QJsonValue(5) << false;
}

void GeoFenceControllerTest::_testFenceJsonVersionMatchesBreachActions()
{
    QFETCH(bool, circle);
    QFETCH(int, version);
    QFETCH(bool, homeCentered);
    QFETCH(QJsonValue, actions);
    QFETCH(bool, loads);

    QJsonObject json;
    if (circle) {
        QGCFenceCircle(home2D(), 100.0, true /* inclusion */).saveToJson(json);
    } else {
        QGCFencePolygon polygon(true /* inclusion */);
        polygon.appendVertex(home2D());
        polygon.appendVertex(home2D().atDistanceAndAzimuth(100, 0));
        polygon.appendVertex(home2D().atDistanceAndAzimuth(100, 90));
        polygon.saveToJson(json);
    }
    json[JsonParsing::jsonVersionKey] = version;
    if (homeCentered) {
        json[QStringLiteral("homeCentered")] = true;
    }
    if (!actions.isUndefined() && !actions.isNull()) {
        json[QStringLiteral("breachActions")] = actions;
    }

    QString errorString;
    bool loaded = false;
    QList<int> loadedActions;
    if (circle) {
        QGCFenceCircle fence;
        fence.setBreachActions({FENCE_ACTION_REPORT});
        loaded = fence.loadFromJson(json, errorString);
        loadedActions = fence.breachActions();
    } else {
        QGCFencePolygon fence(true /* inclusion */);
        fence.setBreachActions({FENCE_ACTION_REPORT});
        loaded = fence.loadFromJson(json, true /* required */, errorString);
        loadedActions = fence.breachActions();
    }
    QCOMPARE(loaded, loads);
    if (loads) {
        // Loading also clears actions the fence had before
        QCOMPARE(loadedActions.isEmpty(), actions.isNull() || actions.isUndefined());
    } else {
        QVERIFY(!errorString.isEmpty());
    }
}

UT_REGISTER_TEST(GeoFenceControllerTest, TestLabel::Integration, TestLabel::MissionManager)
