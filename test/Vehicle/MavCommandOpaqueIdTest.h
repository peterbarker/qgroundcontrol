#pragma once

#include "BaseClasses/VehicleTest.h"
#include "Vehicle.h"

/// Tests MavCommandQueue's use of COMMAND_INT/COMMAND_LONG.command_opaque_id: every command gets a
/// non-zero ID, its retries reuse it, and COMMAND_ACKs are matched by it when the vehicle echoes one.
class MavCommandOpaqueIdTest : public VehicleTestNoInitialConnect
{
    Q_OBJECT

public:
    explicit MavCommandOpaqueIdTest(QObject* parent = nullptr)
        : VehicleTestNoInitialConnect(parent)
    {
        setAutopilotType(MAV_AUTOPILOT_INVALID);
    }

private slots:
    void _testIdAssigned();
    void _testCommandIntIdAssigned();
    void _testRetriesReuseId();
    void _testAckWithWrongIdIgnored();
    void _testAckWithoutIdMatchedByCommand();

private:
    struct Result
    {
        bool called = false;
        mavlink_command_ack_t ack{};
        Vehicle::MavCmdResultFailureCode_t failureCode = Vehicle::MavCmdResultCommandResultOnly;
    };

    static void _resultHandler(void* resultHandlerData, int compId, const mavlink_command_ack_t& ack,
                               Vehicle::MavCmdResultFailureCode_t failureCode);
    static Vehicle::MavCmdAckHandlerInfo_t _handlerInfo(Result& result);
};
