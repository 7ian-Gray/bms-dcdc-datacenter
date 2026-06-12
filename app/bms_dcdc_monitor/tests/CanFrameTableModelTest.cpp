#include "models/CanFrameFilterProxyModel.h"
#include "models/CanFrameTableModel.h"

#include <QDateTime>
#include <QtTest/QtTest>

class CanFrameTableModelTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultFrameDirectionIsRx();
    void initialRowCountIsZero();
    void appendFrameIncreasesRowCount();
    void columnsDisplayExpectedValues();
    void clearRemovesAllFrames();
    void pausedModelSkipsInsertion();
    void maximumCacheDropsOldestFrames();
    void frameAtOutOfRangeIsSafe();
    void formatsStandardAndExtendedIds();
    void formatsDirectionAndPayload();
    void canIdFilterSupportsPartialHexText();
    void directionFilterSupportsRxAndTx();
    void invalidCanIdFilterDoesNotCrash();

private:
    static CanFrame makeFrame(quint32 id,
                              bool extended,
                              CanFrameDirection direction,
                              const QByteArray &payload,
                              quint64 timestampUs = 1718160000123000ULL,
                              quint8 channel = 1,
                              bool remote = false);
};

CanFrame CanFrameTableModelTest::makeFrame(quint32 id,
                                           bool extended,
                                           CanFrameDirection direction,
                                           const QByteArray &payload,
                                           quint64 timestampUs,
                                           quint8 channel,
                                           bool remote)
{
    CanFrame frame;
    frame.id = id;
    frame.channel = channel;
    frame.extended = extended;
    frame.remote = remote;
    frame.direction = direction;
    frame.timestampUs = timestampUs;
    frame.payload = payload;
    return frame;
}

void CanFrameTableModelTest::initialRowCountIsZero()
{
    CanFrameTableModel model;
    QCOMPARE(model.rowCount(), 0);
}

void CanFrameTableModelTest::defaultFrameDirectionIsRx()
{
    CanFrame frame;
    QCOMPARE(frame.direction, CanFrameDirection::Rx);
}

void CanFrameTableModelTest::appendFrameIncreasesRowCount()
{
    CanFrameTableModel model;
    model.appendFrame(makeFrame(0x18E10101u,
                                true,
                                CanFrameDirection::Rx,
                                QByteArray::fromHex("0102030405060708")));

    QCOMPARE(model.rowCount(), 1);
}

void CanFrameTableModelTest::columnsDisplayExpectedValues()
{
    CanFrameTableModel model;
    const CanFrame frame = makeFrame(0x18E10101u,
                                     true,
                                     CanFrameDirection::Rx,
                                     QByteArray::fromHex("0a0b0c0d0e0f1011"),
                                     1718160000123000ULL,
                                     0,
                                     false);
    model.appendFrame(frame);

    QCOMPARE(model.data(model.index(0, CanFrameTableModel::SequenceColumn)).toString(),
             QStringLiteral("1"));
    QCOMPARE(model.data(model.index(0, CanFrameTableModel::TimeColumn)).toString(),
             QDateTime::fromMSecsSinceEpoch(1718160000123LL)
                 .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")));
    QCOMPARE(model.data(model.index(0, CanFrameTableModel::DirectionColumn)).toString(),
             QStringLiteral("RX"));
    QCOMPARE(model.data(model.index(0, CanFrameTableModel::ChannelColumn)).toString(),
             QStringLiteral("CH0"));
    QCOMPARE(model.data(model.index(0, CanFrameTableModel::CanIdColumn)).toString(),
             QStringLiteral("18E10101"));
    QCOMPARE(model.data(model.index(0, CanFrameTableModel::FrameFormatColumn)).toString(),
             QStringLiteral("扩展帧"));
    QCOMPARE(model.data(model.index(0, CanFrameTableModel::FrameTypeColumn)).toString(),
             QStringLiteral("数据帧"));
    QCOMPARE(model.data(model.index(0, CanFrameTableModel::DlcColumn)).toString(),
             QStringLiteral("8"));
    QCOMPARE(model.data(model.index(0, CanFrameTableModel::DataColumn)).toString(),
             QStringLiteral("0A 0B 0C 0D 0E 0F 10 11"));
}

void CanFrameTableModelTest::clearRemovesAllFrames()
{
    CanFrameTableModel model;
    model.appendFrame(makeFrame(0x18E10101u,
                                true,
                                CanFrameDirection::Rx,
                                QByteArray::fromHex("01020304")));

    model.clear();

    QCOMPARE(model.rowCount(), 0);
}

void CanFrameTableModelTest::pausedModelSkipsInsertion()
{
    CanFrameTableModel model;
    model.setPaused(true);
    model.appendFrame(makeFrame(0x18E10101u,
                                true,
                                CanFrameDirection::Rx,
                                QByteArray::fromHex("01020304")));

    QCOMPARE(model.rowCount(), 0);
}

void CanFrameTableModelTest::maximumCacheDropsOldestFrames()
{
    CanFrameTableModel model;
    model.setMaximumFrameCount(3);

    model.appendFrame(makeFrame(0x101u, false, CanFrameDirection::Rx, QByteArray::fromHex("01")));
    model.appendFrame(makeFrame(0x102u, false, CanFrameDirection::Rx, QByteArray::fromHex("02")));
    model.appendFrame(makeFrame(0x103u, false, CanFrameDirection::Rx, QByteArray::fromHex("03")));
    model.appendFrame(makeFrame(0x104u, false, CanFrameDirection::Rx, QByteArray::fromHex("04")));

    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.data(model.index(0, CanFrameTableModel::SequenceColumn)).toString(),
             QStringLiteral("2"));
    QCOMPARE(model.frameAt(0).id, 0x102u);
    QCOMPARE(model.frameAt(2).id, 0x104u);
}

void CanFrameTableModelTest::frameAtOutOfRangeIsSafe()
{
    CanFrameTableModel model;
    QCOMPARE(model.frameAt(-1).id, 0u);
    QCOMPARE(model.frameAt(0).id, 0u);
}

void CanFrameTableModelTest::formatsStandardAndExtendedIds()
{
    QCOMPARE(CanFrameTableModel::formatCanId(
                 makeFrame(0x7AFu, false, CanFrameDirection::Rx, QByteArray())),
             QStringLiteral("7AF"));
    QCOMPARE(CanFrameTableModel::formatCanId(
                 makeFrame(0x18E10101u, true, CanFrameDirection::Rx, QByteArray())),
             QStringLiteral("18E10101"));
}

void CanFrameTableModelTest::formatsDirectionAndPayload()
{
    QCOMPARE(CanFrameTableModel::formatDirection(CanFrameDirection::Rx),
             QStringLiteral("RX"));
    QCOMPARE(CanFrameTableModel::formatDirection(CanFrameDirection::Tx),
             QStringLiteral("TX"));
    QCOMPARE(CanFrameTableModel::formatPayload(QByteArray::fromHex("0aff10")),
             QStringLiteral("0A FF 10"));
}

void CanFrameTableModelTest::canIdFilterSupportsPartialHexText()
{
    CanFrameTableModel model;
    CanFrameFilterProxyModel proxy;
    proxy.setSourceModel(&model);

    model.appendFrame(makeFrame(0x18E10101u, true, CanFrameDirection::Rx, QByteArray::fromHex("01")));
    model.appendFrame(makeFrame(0x18E20101u, true, CanFrameDirection::Tx, QByteArray::fromHex("02")));

    proxy.setCanIdFilterText(QStringLiteral("18E1"));
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.data(proxy.index(0, CanFrameTableModel::CanIdColumn)).toString(),
             QStringLiteral("18E10101"));

    proxy.setCanIdFilterText(QStringLiteral("0x18E20101"));
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.data(proxy.index(0, CanFrameTableModel::CanIdColumn)).toString(),
             QStringLiteral("18E20101"));
}

void CanFrameTableModelTest::directionFilterSupportsRxAndTx()
{
    CanFrameTableModel model;
    CanFrameFilterProxyModel proxy;
    proxy.setSourceModel(&model);

    model.appendFrame(makeFrame(0x18E10101u, true, CanFrameDirection::Rx, QByteArray::fromHex("01")));
    model.appendFrame(makeFrame(0x18E20101u, true, CanFrameDirection::Tx, QByteArray::fromHex("02")));

    proxy.setDirectionFilter(CanFrameFilterProxyModel::DirectionFilter::RxOnly);
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.data(proxy.index(0, CanFrameTableModel::DirectionColumn)).toString(),
             QStringLiteral("RX"));

    proxy.setDirectionFilter(CanFrameFilterProxyModel::DirectionFilter::TxOnly);
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.data(proxy.index(0, CanFrameTableModel::DirectionColumn)).toString(),
             QStringLiteral("TX"));
}

void CanFrameTableModelTest::invalidCanIdFilterDoesNotCrash()
{
    CanFrameTableModel model;
    CanFrameFilterProxyModel proxy;
    proxy.setSourceModel(&model);

    model.appendFrame(makeFrame(0x18E10101u, true, CanFrameDirection::Rx, QByteArray::fromHex("01")));
    proxy.setCanIdFilterText(QStringLiteral("0x18G1"));

    QVERIFY(!proxy.isCanIdFilterValid());
    QCOMPARE(proxy.rowCount(), 0);
}

QTEST_MAIN(CanFrameTableModelTest)

#include "CanFrameTableModelTest.moc"
