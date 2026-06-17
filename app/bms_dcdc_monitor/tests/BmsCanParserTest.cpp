#include "protocol/BmsCanParser.h"

#include <QtTest/QtTest>

class BmsCanParserTest : public QObject
{
    Q_OBJECT

private slots:
    void parsesValidExtendedFrame();
    void rejectsStandardFrame();
    void rejectsRemoteFrame();
    void rejectsShortPayload();
    void rejectsLongPayload();
    void rejectsWrongDestinationAddress();
    void rejectsWrongSourceAddress();
    void ignoresUnrelatedCanId();
    void parsesNegativeCurrent();
    void failedParseDoesNotPolluteExistingData();

private:
    static CanFrame basicInfoFrame();
};

CanFrame BmsCanParserTest::basicInfoFrame()
{
    CanFrame frame;
    frame.id = 0x18E10101u;
    frame.extended = true;
    frame.remote = false;
    frame.payload = QByteArray::fromHex("021d94fe0e03c803");
    return frame;
}

void BmsCanParserTest::parsesValidExtendedFrame()
{
    const BmsCanParser parser;
    const BmsParseResult result = parser.parseFrame(basicInfoFrame());

    QCOMPARE(result.status, BmsParseResult::Status::Updated);
    QCOMPARE(result.updateFlags, static_cast<quint32>(BmsCanParser::BasicInfoUpdated));
    QCOMPARE(result.data.packVoltage, 742.6);
    QCOMPARE(result.data.soc, 78.2);
    QCOMPARE(result.data.soh, 96.8);
}

void BmsCanParserTest::rejectsStandardFrame()
{
    CanFrame frame = basicInfoFrame();
    frame.extended = false;

    const BmsParseResult result = BmsCanParser().parseFrame(frame);

    QCOMPARE(result.status, BmsParseResult::Status::InvalidFrame);
}

void BmsCanParserTest::rejectsRemoteFrame()
{
    CanFrame frame = basicInfoFrame();
    frame.remote = true;

    const BmsParseResult result = BmsCanParser().parseFrame(frame);

    QCOMPARE(result.status, BmsParseResult::Status::InvalidFrame);
}

void BmsCanParserTest::rejectsShortPayload()
{
    CanFrame frame = basicInfoFrame();
    frame.payload = QByteArray::fromHex("021d94fe0e03c8");

    const BmsParseResult result = BmsCanParser().parseFrame(frame);

    QCOMPARE(result.status, BmsParseResult::Status::InvalidFrame);
}

void BmsCanParserTest::rejectsLongPayload()
{
    CanFrame frame = basicInfoFrame();
    frame.payload = QByteArray::fromHex("021d94fe0e03c80300");

    const BmsParseResult result = BmsCanParser().parseFrame(frame);

    QCOMPARE(result.status, BmsParseResult::Status::InvalidFrame);
}

void BmsCanParserTest::rejectsWrongDestinationAddress()
{
    CanFrame frame = basicInfoFrame();
    frame.id = 0x18E10201u;

    const BmsParseResult result = BmsCanParser().parseFrame(frame);

    QCOMPARE(result.status, BmsParseResult::Status::NotMatched);
}

void BmsCanParserTest::rejectsWrongSourceAddress()
{
    CanFrame frame = basicInfoFrame();
    frame.id = 0x18E10102u;

    const BmsParseResult result = BmsCanParser().parseFrame(frame);

    QCOMPARE(result.status, BmsParseResult::Status::NotMatched);
}

void BmsCanParserTest::ignoresUnrelatedCanId()
{
    CanFrame frame = basicInfoFrame();
    frame.id = 0x18F00101u;

    const BmsParseResult result = BmsCanParser().parseFrame(frame);

    QCOMPARE(result.status, BmsParseResult::Status::NotMatched);
}

void BmsCanParserTest::parsesNegativeCurrent()
{
    const BmsParseResult result = BmsCanParser().parseFrame(basicInfoFrame());

    QCOMPARE(result.status, BmsParseResult::Status::Updated);
    QCOMPARE(result.data.packCurrent, -36.4);
}

void BmsCanParserTest::failedParseDoesNotPolluteExistingData()
{
    const BmsCanParser parser;
    BmsData existing;
    existing.packVoltage = 123.4;
    existing.packCurrent = 56.7;
    existing.soc = 89.0;
    existing.soh = 91.2;

    CanFrame frame = basicInfoFrame();
    frame.payload = QByteArray::fromHex("021d94fe0e03c8");

    const BmsCanParser::UpdateFlags updates = parser.parse(frame, existing);

    QCOMPARE(updates, BmsCanParser::NoUpdate);
    QCOMPARE(existing.packVoltage, 123.4);
    QCOMPARE(existing.packCurrent, 56.7);
    QCOMPARE(existing.soc, 89.0);
    QCOMPARE(existing.soh, 91.2);
}

QTEST_MAIN(BmsCanParserTest)

#include "BmsCanParserTest.moc"
