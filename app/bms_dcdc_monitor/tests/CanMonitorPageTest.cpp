#include "pages/CanMonitorPage.h"

#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest/QtTest>

class CanMonitorPageTest : public QObject
{
    Q_OBJECT

private slots:
    void sendAreaAlwaysConstructsDataFrame();
};

void CanMonitorPageTest::sendAreaAlwaysConstructsDataFrame()
{
    CanMonitorPage page;
    page.setConnectionState(CanConnectionState::ChannelStarted);

    auto *frameTypeCombo =
        page.findChild<QComboBox *>(QStringLiteral("sendFrameTypeComboBox"));
    auto *canIdEdit =
        page.findChild<QLineEdit *>(QStringLiteral("sendCanIdLineEdit"));
    auto *dataEdit =
        page.findChild<QLineEdit *>(QStringLiteral("sendDataLineEdit"));
    auto *sendButton =
        page.findChild<QPushButton *>(QStringLiteral("sendFrameButton"));

    QVERIFY(frameTypeCombo != nullptr);
    QVERIFY(canIdEdit != nullptr);
    QVERIFY(dataEdit != nullptr);
    QVERIFY(sendButton != nullptr);

    QCOMPARE(frameTypeCombo->count(), 1);
    QVERIFY(!frameTypeCombo->isEnabled());
    QCOMPARE(frameTypeCombo->currentData().toBool(), false);

    canIdEdit->setText(QStringLiteral("18E10101"));
    dataEdit->setText(QStringLiteral("01 02 03 04"));

    QSignalSpy sendSpy(&page, &CanMonitorPage::sendFrameRequested);
    QVERIFY(sendButton->isEnabled());

    sendButton->click();

    QCOMPARE(sendSpy.count(), 1);
    const CanFrame frame = qvariant_cast<CanFrame>(sendSpy.takeFirst().at(0));
    QCOMPARE(frame.remote, false);
    QCOMPARE(frame.id, 0x18E10101u);
    QCOMPARE(frame.extended, true);
    QCOMPARE(frame.direction, CanFrameDirection::Tx);
    QCOMPARE(frame.payload, QByteArray::fromHex("01020304"));
}

QTEST_MAIN(CanMonitorPageTest)

#include "CanMonitorPageTest.moc"
