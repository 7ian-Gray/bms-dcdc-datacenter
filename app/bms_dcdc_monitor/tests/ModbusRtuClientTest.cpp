#define private public
#include "communication/ModbusRtuClient.h"
#undef private

#include <QSignalSpy>
#include <QtTest/QtTest>

class ModbusRtuClientTest : public QObject
{
    Q_OBJECT

private slots:
    void buildsReadHoldingRegistersRequestGoldenVector();
    void decodesValidResponse();
    void rejectsWrongSlaveAddress();
    void rejectsWrongFunctionCode();
    void rejectsMismatchedByteCount();
    void rejectsCrcMismatch();
    void recognizesExceptionResponse();
    void waitsForHalfFrame();
    void parsesStickyFrames();
    void resynchronizesAfterLeadingNoise();
    void pendingPollDoesNotSendSecondRequest();
    void timeoutRetriesPendingRequest();
    void timeoutAfterMaxRetriesClearsPending();
    void closeClearsPendingAndTimers();

private:
    static PendingModbusRequest pendingRequest();
    static QByteArray validResponse();
};

PendingModbusRequest ModbusRtuClientTest::pendingRequest()
{
    PendingModbusRequest pending;
    pending.slaveAddress = 0x11;
    pending.functionCode = ModbusRtuClient::ReadHoldingRegistersFunction;
    pending.startRegister = 0x0010;
    pending.registerCount = 2;
    pending.requestFrame =
        ModbusRtuClient::buildReadHoldingRegistersRequest(pending.slaveAddress,
                                                          pending.startRegister,
                                                          pending.registerCount);
    return pending;
}

QByteArray ModbusRtuClientTest::validResponse()
{
    return QByteArray::fromHex("110304000a01024ba1");
}

void ModbusRtuClientTest::buildsReadHoldingRegistersRequestGoldenVector()
{
    const QByteArray request =
        ModbusRtuClient::buildReadHoldingRegistersRequest(0x11, 0x0010, 2);

    QCOMPARE(request, QByteArray::fromHex("110300100002c75e"));
}

void ModbusRtuClientTest::decodesValidResponse()
{
    const ModbusResponseValidation result =
        ModbusRtuClient::validateResponse(validResponse(), pendingRequest());

    QCOMPARE(result.status, ModbusResponseValidation::Status::Complete);
    QCOMPARE(result.registers.size(), 2);
    QCOMPARE(result.registers.at(0), static_cast<quint16>(0x000a));
    QCOMPARE(result.registers.at(1), static_cast<quint16>(0x0102));
}

void ModbusRtuClientTest::rejectsWrongSlaveAddress()
{
    const ModbusResponseValidation result =
        ModbusRtuClient::validateResponse(QByteArray::fromHex("120304000a010278a1"),
                                          pendingRequest());

    QCOMPARE(result.status, ModbusResponseValidation::Status::ResponseMismatch);
}

void ModbusRtuClientTest::rejectsWrongFunctionCode()
{
    const ModbusResponseValidation result =
        ModbusRtuClient::validateResponse(QByteArray::fromHex("110404000a01024a16"),
                                          pendingRequest());

    QCOMPARE(result.status, ModbusResponseValidation::Status::ResponseMismatch);
}

void ModbusRtuClientTest::rejectsMismatchedByteCount()
{
    const ModbusResponseValidation result =
        ModbusRtuClient::validateResponse(QByteArray::fromHex("110302000af980"),
                                          pendingRequest());

    QCOMPARE(result.status, ModbusResponseValidation::Status::ResponseMismatch);
}

void ModbusRtuClientTest::rejectsCrcMismatch()
{
    QByteArray response = validResponse();
    response[response.size() - 1] = static_cast<char>(0x00);

    const ModbusResponseValidation result =
        ModbusRtuClient::validateResponse(response, pendingRequest());

    QCOMPARE(result.status, ModbusResponseValidation::Status::CrcMismatch);
}

void ModbusRtuClientTest::recognizesExceptionResponse()
{
    const ModbusResponseValidation result =
        ModbusRtuClient::validateResponse(QByteArray::fromHex("118302c134"),
                                          pendingRequest());

    QCOMPARE(result.status, ModbusResponseValidation::Status::ExceptionResponse);
    QCOMPARE(result.exceptionCode, static_cast<quint8>(0x02));
}

void ModbusRtuClientTest::waitsForHalfFrame()
{
    const ModbusResponseValidation result =
        ModbusRtuClient::validateResponse(QByteArray::fromHex("110304000a"),
                                          pendingRequest());

    QCOMPARE(result.status, ModbusResponseValidation::Status::NeedMoreData);
}

void ModbusRtuClientTest::parsesStickyFrames()
{
    ModbusRtuClient client;
    QSignalSpy receivedSpy(&client, &ModbusRtuClient::holdingRegistersReceived);

    client.pendingRequest_ = pendingRequest();
    client.receiveBuffer_ = validResponse() + QByteArray::fromHex("120304000a010278a1");
    client.parseBufferedFrames();

    QCOMPARE(receivedSpy.count(), 1);
    QVERIFY(!client.pendingRequest_.has_value());
    QVERIFY(client.receiveBuffer_.isEmpty());
}

void ModbusRtuClientTest::resynchronizesAfterLeadingNoise()
{
    ModbusRtuClient client;
    QSignalSpy receivedSpy(&client, &ModbusRtuClient::holdingRegistersReceived);

    client.pendingRequest_ = pendingRequest();
    client.receiveBuffer_ = QByteArray::fromHex("0012ff") + validResponse();
    client.parseBufferedFrames();

    QCOMPARE(receivedSpy.count(), 1);
    QVERIFY(client.receiveBuffer_.isEmpty());
}

void ModbusRtuClientTest::pendingPollDoesNotSendSecondRequest()
{
    ModbusRtuClient client;
    QSignalSpy errorSpy(&client, &ModbusRtuClient::protocolError);

    client.pendingRequest_ = pendingRequest();
    client.onPollTimeout();

    QVERIFY(client.pendingRequest_.has_value());
    QCOMPARE(errorSpy.count(), 0);
}

void ModbusRtuClientTest::timeoutRetriesPendingRequest()
{
    ModbusRtuClient client;
    client.setMaxRetries(2);
    client.pendingRequest_ = pendingRequest();
    QSignalSpy retrySpy(&client, &ModbusRtuClient::requestRetried);

    client.onRequestTimeout();

    QVERIFY(client.pendingRequest_.has_value());
    QCOMPARE(client.pendingRequest_->retryCount, 1);
    QCOMPARE(retrySpy.count(), 1);
}

void ModbusRtuClientTest::timeoutAfterMaxRetriesClearsPending()
{
    ModbusRtuClient client;
    client.setMaxRetries(1);
    PendingModbusRequest pending = pendingRequest();
    pending.retryCount = 1;
    client.pendingRequest_ = pending;
    QSignalSpy timeoutSpy(&client, &ModbusRtuClient::requestTimedOut);

    client.onRequestTimeout();

    QVERIFY(!client.pendingRequest_.has_value());
    QCOMPARE(timeoutSpy.count(), 1);
}

void ModbusRtuClientTest::closeClearsPendingAndTimers()
{
    ModbusRtuClient client;
    client.pendingRequest_ = pendingRequest();
    client.receiveBuffer_ = QByteArray::fromHex("110304");
    client.pollTimer_.start(1000);
    client.requestTimeoutTimer_.start(1000);

    client.close();

    QVERIFY(!client.pendingRequest_.has_value());
    QVERIFY(client.receiveBuffer_.isEmpty());
    QVERIFY(!client.pollTimer_.isActive());
    QVERIFY(!client.requestTimeoutTimer_.isActive());
}

QTEST_MAIN(ModbusRtuClientTest)

#include "ModbusRtuClientTest.moc"
