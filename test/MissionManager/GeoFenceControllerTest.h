#pragma once

#include "MissionTest.h"

/// Tests GeoFenceController support for home-centered circular fences
/// (MAV_CMD_NAV_FENCE_HOME_CIRCLE_INCLUSION) and per-area breach actions
/// (MAV_CMD_NAV_SET_FENCE_ACTION) against an ArduPilot MockLink.
class GeoFenceControllerTest : public MissionTest
{
    Q_OBJECT

public:
    explicit GeoFenceControllerTest(QObject* parent = nullptr);

private slots:
    void _testHomeCircleSupportedVersion_data();
    void _testHomeCircleSupportedVersion();
    void _testHomeCircleVehicleRoundTrip();
    void _testHomeCircleFollowsHomeWithoutDirty();
    void _testUploadRefusedWhenHomeCircleUnsupported();
    void _testHomeCircleJsonRoundTrip();
    void _testCircleJsonVersionMatchesHomeCentered_data();
    void _testCircleJsonVersionMatchesHomeCentered();
    void _testBreachActionsSupportedVersion_data();
    void _testBreachActionsSupportedVersion();
    void _testSetBreachAction();
    void _testBreachActionsVehicleRoundTrip();
    void _testUploadRefusedWhenBreachActionUnsupported();
    void _testBreachActionsJsonRoundTrip();
    void _testFenceJsonVersionMatchesBreachActions_data();
    void _testFenceJsonVersionMatchesBreachActions();
};
