// An icon's object as the 3D scenes take it (IconModel), with a stand-in for
// the icon helper that hands every picture the same object: asked for once,
// kept in the cache and read from there next time, laid where the picture
// lies in the scene's square; a picture the helper fails is left without.
#include "IconModel.h"

#include <QFile>
#include <QGuiApplication>
#include <QPainter>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace ArchDock;

class IconModelTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_root;
    QString m_log;

    // A picture of its own colour on a clear ground, wider than tall.
    QString picture(const QString &name, QColor colour)
    {
        QImage image(300, 200, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(colour);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QRectF(60, 10, 180, 180));
        painter.end();
        const QString path = m_root.filePath(name);
        image.save(path);
        return path;
    }

    int helperRuns() const
    {
        QFile log(m_log);
        return log.open(QIODevice::ReadOnly) ? int(log.readAll().count('\n')) : 0;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_root.isValid());
        qputenv("XDG_CACHE_HOME", m_root.filePath(QStringLiteral("cache")).toLocal8Bit());
        // The models only need to be there.
        QDir().mkpath(m_root.filePath(QStringLiteral("models")));
        for (const char *name : {"triposr-network.onnx", "triposr-field.onnx"})
        {
            QFile file(m_root.filePath(QStringLiteral("models/")) + QString::fromLatin1(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        qputenv("ARCHDOCK_ICON_OBJECT_MODELS", m_root.filePath(QStringLiteral("models")).toLocal8Bit());
        // The object every picture is given: a triangle 40 across, 10 deep,
        // on the picture's left half.
        IconObjectMesh object;
        object.vertices = {-40, -20, 10, 0, 0, 1, 0.1f, 0.3f, 1, 0, 0, 1,
                           0, -20, 10, 0, 0, 1, 0.5f, 0.3f, 0, 1, 0, 1,
                           -20, 20, -10, 0, 0, 1, 0.3f, 0.7f, 0, 0, 1, 1};
        object.indices = {0, 1, 2};
        object.minimum = QVector3D(-40, -20, -10);
        object.maximum = QVector3D(0, 20, 10);
        object.depth = 10;
        const QString prepared = m_root.filePath(QStringLiteral("prepared.adicon"));
        QVERIFY(saveIconObject(object, prepared));
        // The stand-in helper: each picture's object is the prepared one,
        // none at all while a file named `fail` is there.
        m_log = m_root.filePath(QStringLiteral("helper.log"));
        const QString helper = m_root.filePath(QStringLiteral("helper.sh"));
        QFile script(helper);
        QVERIFY(script.open(QIODevice::WriteOnly));
        script.write(QStringLiteral("#!/bin/sh\necho \"$@\" >> '%1'\n"
                                    "[ -e '%2' ] && exit 4\n"
                                    "while [ $# -ge 2 ]; do cp '%3' \"$2\"; shift 2; done\n")
                         .arg(m_log, m_root.filePath(QStringLiteral("fail")), prepared)
                         .toLocal8Bit());
        script.close();
        QVERIFY(script.setPermissions(script.permissions() | QFileDevice::ExeOwner));
        qputenv("ARCHDOCK_ICON_OBJECT_HELPER", helper.toLocal8Bit());
        QVERIFY(IconModel::available());
    }

    // Asked once, the object comes from the helper, laid where the picture
    // lies: here the scene's middle half, so 40 across becomes 20, the
    // depth 10 becomes 5, and where on the picture each point lies becomes
    // where on the square.
    void madeOnceAndLaidInTheFrame()
    {
        IconModel model;
        QSignalSpy making(&model, &IconModel::makingChanged);
        model.setFrame(QRectF(0.25, 0.25, 0.5, 0.5));
        model.setIconSource(picture(QStringLiteral("red.png"), Qt::red));
        QTRY_VERIFY_WITH_TIMEOUT(model.ready(), 10000);
        // It was being made, and is no longer.
        QCOMPARE(making.count(), 2);
        QVERIFY(!model.making());
        QCOMPARE(helperRuns(), 1);
        QCOMPARE(model.triangles(), 1);
        QCOMPARE(model.depth(), 5.0);
        const auto mesh = model.mesh();
        QCOMPARE(mesh->vertices[0], -20.0f);
        QCOMPARE(mesh->vertices[1], -10.0f);
        QCOMPARE(mesh->vertices[2], 5.0f);
        QCOMPARE(mesh->vertices[6], 0.25f + 0.1f * 0.5f);
        QCOMPARE(mesh->vertices[7], 1 - (0.25f + 0.7f * 0.5f));
        QVERIFY(qFuzzyCompare(mesh->minimum, QVector3D(-20, -10, -5)));
        // A new frame lays it anew, not made again.
        model.setFrame(QRectF(0, 0, 1, 1));
        QCOMPARE(model.mesh()->vertices[0], -40.0f);
        QCOMPARE(model.depth(), 10.0);
        QCOMPARE(helperRuns(), 1);

        // The same picture elsewhere is read, not made again.
        IconModel again;
        again.setIconSource(picture(QStringLiteral("red-again.png"), Qt::red));
        QTRY_VERIFY_WITH_TIMEOUT(again.ready(), 5000);
        QCOMPARE(helperRuns(), 1);
    }

    // A picture the helper fails is left without an object, and is not
    // asked for again while the program runs.
    void failedPicturesStayWithout()
    {
        QFile fail(m_root.filePath(QStringLiteral("fail")));
        QVERIFY(fail.open(QIODevice::WriteOnly));
        fail.close();
        const QString blue = picture(QStringLiteral("blue.png"), Qt::blue);
        IconModel model;
        QSignalSpy making(&model, &IconModel::makingChanged);
        model.setIconSource(blue);
        QTRY_COMPARE_WITH_TIMEOUT(making.count(), 2, 10000);
        QVERIFY(!model.making());
        QVERIFY(!model.ready());
        const int runs = helperRuns();
        QVERIFY(fail.remove());
        IconModel again;
        again.setIconSource(blue);
        QTest::qWait(500);
        QVERIFY(!again.ready());
        QCOMPARE(helperRuns(), runs);
    }

    // Nothing to show, nothing asked.
    void noSourceNoObject()
    {
        IconModel model;
        model.setIconSource(QString());
        QTest::qWait(100);
        QVERIFY(!model.ready());
        QVERIFY(!model.making());
    }
};

QTEST_MAIN(IconModelTest)
#include "IconModelTest.moc"
