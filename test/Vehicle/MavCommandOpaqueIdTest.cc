#include "MavCommandOpaqueIdTest.h"

#include <QtCore/QRegularExpression>
#include <QtTest/QTest>

#include "Fixtures/RAIIFixtures.h"
#include "MavCommandQueue.h"
#include "MockLink.h"

void MavCommandOpaqueIdTest::_resultHandler(void* resultHandlerData, int /*compId*/, const mavlink_command_ack_t& ack,
                                            Vehicle::MavCmdResultFailureCode_t failureCode)
{
    auto* result = static_cast<Result*>(resultHandlerData);
    result->called = true;
    result->ack = ack;
    result->failureCode = failureCode;
}

Vehicle::MavCmdAckHandlerInfo_t MavCommandOpaqueIdTest::_handlerInfo(Result& result)
{
    Vehicle::MavCmdAckHandlerInfo_t handlerInfo = {};
    handlerInfo.resultHandler = _resultHandler;
    handlerInfo.resultHandlerData = &result;
    return handlerInfo;
}

void MavCommandOpaqueIdTest::_testIdAssigned()
{
    constexpr MAV_CMD command = MockLink::MAV_CMD_MOCKLINK_ALWAYS_RESULT_ACCEPTED;
    _mockLink->clearReceivedMavCommandCounts();

    Result first;
    Vehicle::MavCmdAckHandlerInfo_t handlerInfo = _handlerInfo(first);
    vehicle()->sendMavCommandWithHandler(&handlerInfo, MAV_COMP_ID_AUTOPILOT1, command);
    QVERIFY_TRUE_WAIT(first.called, TestTimeout::longMs());

    Result second;
    handlerInfo = _handlerInfo(second);
    vehicle()->sendMavCommandWithHandler(&handlerInfo, MAV_COMP_ID_AUTOPILOT1, command);
    QVERIFY_TRUE_WAIT(second.called, TestTimeout::longMs());

    const QList<uint32_t> ids = _mockLink->receivedCommandOpaqueIds(command);
    QCOMPARE(ids.count(), 2);
    QVERIFY(ids[0] != 0);
    QVERIFY(ids[1] != 0);
    QVERIFY(ids[0] != ids[1]);

    // The ACKs echoing those IDs were matched to their commands
    QCOMPARE(first.ack.result, MAV_RESULT_ACCEPTED);
    QCOMPARE(first.ack.command_opaque_id, ids[0]);
    QCOMPARE(second.ack.result, MAV_RESULT_ACCEPTED);
    QCOMPARE(second.ack.command_opaque_id, ids[1]);
}

void MavCommandOpaqueIdTest::_testCommandIntIdAssigned()
{
    constexpr MAV_CMD command = MAV_CMD_DO_SET_ROI_LOCATION;
    _mockLink->clearReceivedMavCommandCounts();

    Result result;
    const Vehicle::MavCmdAckHandlerInfo_t handlerInfo = _handlerInfo(result);
    vehicle()->sendMavCommandIntWithHandler(&handlerInfo, MAV_COMP_ID_AUTOPILOT1, command, MAV_FRAME_GLOBAL, 0, 0, 0, 0,
                                            47.0, 8.0, 100);
    QVERIFY_TRUE_WAIT(result.called, TestTimeout::longMs());

    const QList<uint32_t> ids = _mockLink->receivedCommandOpaqueIds(command);
    QCOMPARE(ids.count(), 1);
    QVERIFY(ids[0] != 0);
    QCOMPARE(result.ack.result, MAV_RESULT_ACCEPTED);
    QCOMPARE(result.ack.command_opaque_id, ids[0]);
}

void MavCommandOpaqueIdTest::_testRetriesReuseId()
{
    TestFixtures::MavCommandAckTimeoutFixture shortAckTimeout;
    ignoreLogMessage("Vehicle.MavCommandQueue", QtWarningMsg,
                     QRegularExpression("Giving up sending command after max retries:"));

    constexpr MAV_CMD command = MockLink::MAV_CMD_MOCKLINK_NO_RESPONSE;
    _mockLink->clearReceivedMavCommandCounts();

    Result result;
    const Vehicle::MavCmdAckHandlerInfo_t handlerInfo = _handlerInfo(result);
    vehicle()->sendMavCommandWithHandler(&handlerInfo, MAV_COMP_ID_AUTOPILOT1, command);
    QVERIFY_TRUE_WAIT(result.called, TestTimeout::longMs());
    QCOMPARE(result.failureCode, Vehicle::MavCmdResultFailureNoResponseToCommand);

    const QList<uint32_t> ids = _mockLink->receivedCommandOpaqueIds(command);
    QCOMPARE(ids.count(), MavCommandQueue::kMaxRetryCount);
    QVERIFY(ids[0] != 0);
    for (const uint32_t id : ids) {
        QCOMPARE(id, ids[0]);
    }
}

void MavCommandOpaqueIdTest::_testAckWithWrongIdIgnored()
{
    // MockLink never answers this command, so the only ACKs are the ones sent below. It is retried
    // (with the same ID), so a slow test run can't time it out before the matching ACK is sent.
    constexpr MAV_CMD command = MockLink::MAV_CMD_MOCKLINK_NO_RESPONSE;
    _mockLink->clearReceivedMavCommandCounts();

    Result result;
    const Vehicle::MavCmdAckHandlerInfo_t handlerInfo = _handlerInfo(result);
    vehicle()->sendMavCommandWithHandler(&handlerInfo, MAV_COMP_ID_AUTOPILOT1, command);
    QVERIFY_TRUE_WAIT(!_mockLink->receivedCommandOpaqueIds(command).isEmpty(), TestTimeout::shortMs());
    const uint32_t id = _mockLink->receivedCommandOpaqueIds(command).first();
    QVERIFY(id != 0);

    // An ACK for the same command but another (non-zero) ID belongs to some other sender's command.
    // The matching ACK follows it on the same link, so it is handled first: had it been accepted,
    // the handler would have seen DENIED and wrongId, and the matching ACK would find no command.
    const uint32_t wrongId = (id + 1 != 0) ? id + 1 : 1;
    _mockLink->sendUnexpectedCommandAck(command, MAV_RESULT_DENIED, wrongId);
    _mockLink->sendUnexpectedCommandAck(command, MAV_RESULT_ACCEPTED, id);
    QVERIFY_TRUE_WAIT(result.called, TestTimeout::shortMs());
    QCOMPARE(result.ack.result, MAV_RESULT_ACCEPTED);
    QCOMPARE(result.ack.command_opaque_id, id);
    QCOMPARE(vehicle()->_findMavCommandListEntryIndex(MAV_COMP_ID_AUTOPILOT1, command), -1);
}

void MavCommandOpaqueIdTest::_testAckWithoutIdMatchedByCommand()
{
    // Firmware which predates command_opaque_id acknowledges with 0. MockLink is reconnected for
    // every test function, so this doesn't leak into other tests.
    _mockLink->setEchoCommandOpaqueId(false);

    constexpr MAV_CMD command = MockLink::MAV_CMD_MOCKLINK_ALWAYS_RESULT_ACCEPTED;
    _mockLink->clearReceivedMavCommandCounts();

    Result result;
    const Vehicle::MavCmdAckHandlerInfo_t handlerInfo = _handlerInfo(result);
    vehicle()->sendMavCommandWithHandler(&handlerInfo, MAV_COMP_ID_AUTOPILOT1, command);
    QVERIFY_TRUE_WAIT(result.called, TestTimeout::longMs());
    QCOMPARE(result.ack.result, MAV_RESULT_ACCEPTED);
    QCOMPARE(result.ack.command_opaque_id, 0u);
    QCOMPARE(vehicle()->_findMavCommandListEntryIndex(MAV_COMP_ID_AUTOPILOT1, command), -1);
}

UT_REGISTER_TEST(MavCommandOpaqueIdTest, TestLabel::Integration, TestLabel::Vehicle)
