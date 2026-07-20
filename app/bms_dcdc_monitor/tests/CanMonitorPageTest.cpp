// The page does not assign objectNames, so findChild lookup is ambiguous;
// widget members are reached directly instead of changing production code.
#define private public
#include "pages/CanMonitorPage.h"
#undef private

#include <QAbstractButton>
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

    QVERIFY(page.sendFrameTypeComboBox_ != nullptr);
    QCOMPARE(page.sendFrameTypeComboBox_->count(), 1);
    QVERIFY(!page.sendFrameTypeComboBox_->isEnabled());
    QCOMPARE(page.sendFrameTypeComboBox_->currentData().toBool(), false);

    page.sendCanIdLineEdit_->setText(QStringLiteral("18E10101"));
    page.sendDataLineEdit_->setText(QStringLiteral("01 02 03 04"));

    QSignalSpy sendSpy(&page, &CanMonitorPage::sendFrameRequested);
    QPushButton *sendButton = page.sendFrameButton_;
    QVERIFY(sendButton != nullptr);
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
