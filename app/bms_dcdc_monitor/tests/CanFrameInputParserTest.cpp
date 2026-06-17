#include "communication/CanFrameInputParser.h"

#include <QtTest/QtTest>

class CanFrameInputParserTest : public QObject
{
    Q_OBJECT

private slots:
    void parsesExtendedIdWithoutPrefix();
    void parsesExtendedIdWithPrefix();
    void acceptsMaxStandardId();
    void rejectsStandardIdOverflow();
    void acceptsMaxExtendedId();
    void rejectsExtendedIdOverflow();
    void acceptsEmptyData();
    void acceptsSpacedData();
    void acceptsCompactData();
    void rejectsNonHexData();
    void rejectsOddHexData();
    void rejectsPayloadLongerThanEightBytes();
    void rejectsRemoteFrameRequests();
};

void CanFrameInputParserTest::parsesExtendedIdWithoutPrefix()
{
    const auto result = CanFrameInputParser::parse({QStringLiteral("18E10101"), QString(), 0, true, false});
    QVERIFY(result.ok);
    QCOMPARE(result.frame.id, 0x18E10101u);
}

void CanFrameInputParserTest::parsesExtendedIdWithPrefix()
{
    const auto result = CanFrameInputParser::parse({QStringLiteral("0x18E10101"), QString(), 0, true, false});
    QVERIFY(result.ok);
    QCOMPARE(result.frame.id, 0x18E10101u);
}

void CanFrameInputParserTest::acceptsMaxStandardId()
{
    const auto result = CanFrameInputParser::parse({QStringLiteral("0x7FF"), QString(), 0, false, false});
    QVERIFY(result.ok);
    QCOMPARE(result.frame.id, 0x7FFu);
}

void CanFrameInputParserTest::rejectsStandardIdOverflow()
{
    const auto result = CanFrameInputParser::parse({QStringLiteral("0x800"), QString(), 0, false, false});
    QVERIFY(!result.ok);
}

void CanFrameInputParserTest::acceptsMaxExtendedId()
{
    const auto result = CanFrameInputParser::parse({QStringLiteral("0x1FFFFFFF"), QString(), 0, true, false});
    QVERIFY(result.ok);
    QCOMPARE(result.frame.id, 0x1FFFFFFFu);
}

void CanFrameInputParserTest::rejectsExtendedIdOverflow()
{
    const auto result = CanFrameInputParser::parse({QStringLiteral("0x20000000"), QString(), 0, true, false});
    QVERIFY(!result.ok);
}

void CanFrameInputParserTest::acceptsEmptyData()
{
    const auto result = CanFrameInputParser::parse({QStringLiteral("123"), QString(), 0, false, false});
    QVERIFY(result.ok);
    QVERIFY(result.frame.payload.isEmpty());
}

void CanFrameInputParserTest::acceptsSpacedData()
{
    const auto result = CanFrameInputParser::parse({QStringLiteral("123"), QStringLiteral("01 02 0A FF"), 0, false, false});
    QVERIFY(result.ok);
    QCOMPARE(result.frame.payload, QByteArray::fromHex("01020aff"));
}

void CanFrameInputParserTest::acceptsCompactData()
{
    const auto result = CanFrameInputParser::parse({QStringLiteral("123"), QStringLiteral("01020AFF"), 0, false, false});
    QVERIFY(result.ok);
    QCOMPARE(result.frame.payload, QByteArray::fromHex("01020aff"));
}

void CanFrameInputParserTest::rejectsNonHexData()
{
    const auto result = CanFrameInputParser::parse({QStringLiteral("123"), QStringLiteral("01 ZZ"), 0, false, false});
    QVERIFY(!result.ok);
}

void CanFrameInputParserTest::rejectsOddHexData()
{
    const auto result = CanFrameInputParser::parse({QStringLiteral("123"), QStringLiteral("010"), 0, false, false});
    QVERIFY(!result.ok);
}

void CanFrameInputParserTest::rejectsPayloadLongerThanEightBytes()
{
    const auto result = CanFrameInputParser::parse({QStringLiteral("123"), QStringLiteral("010203040506070809"), 0, false, false});
    QVERIFY(!result.ok);
}

void CanFrameInputParserTest::rejectsRemoteFrameRequests()
{
    const auto result = CanFrameInputParser::parse({QStringLiteral("123"), QString(), 0, false, true});
    QVERIFY(!result.ok);
    QCOMPARE(result.errorMessage, QStringLiteral("当前阶段仅支持经典 CAN 数据帧，暂不支持远程帧"));
}

QTEST_MAIN(CanFrameInputParserTest)

#include "CanFrameInputParserTest.moc"
