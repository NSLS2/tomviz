/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <QDebug>
#include <QProcess>
#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>
#include <QTest>

#include "DockerUtilities.h"

using namespace tomviz;

class DockerUtilitiesTest : public QObject
{
  Q_OBJECT

private:
  int m_invocationTimeout = 30000;

  docker::DockerRunInvocation* run(
    const QString& image, const QString& entryPoint = QString(),
    const QStringList& containerArgs = QStringList(),
    const QMap<QString, QString>& bindMounts = QMap<QString, QString>())
  {
    auto runInvocation =
      docker::run(image, entryPoint, containerArgs, bindMounts);
    QSignalSpy runError(runInvocation, &docker::DockerPullInvocation::error);
    QSignalSpy runFinished(runInvocation,
                           &docker::DockerPullInvocation::finished);
    runFinished.wait(m_invocationTimeout);

    return runInvocation;
  }

  void remove(const QString& containerId)
  {
    auto removeInvocation = docker::remove(containerId);
    QSignalSpy removeError(removeInvocation,
                           &docker::DockerRemoveInvocation::error);
    QSignalSpy removeFinished(removeInvocation,
                              &docker::DockerRemoveInvocation::finished);
    QVERIFY(removeFinished.wait(m_invocationTimeout));
    removeInvocation->deleteLater();
  }

  void pull(const QString& image)
  {
    auto alpinePullInvocation = docker::pull(image);
    QSignalSpy alpinePullError(alpinePullInvocation,
                               &docker::DockerPullInvocation::error);
    QSignalSpy alpinePullFinished(alpinePullInvocation,
                                  &docker::DockerPullInvocation::finished);
    QVERIFY(alpinePullFinished.wait(m_invocationTimeout));
  }

private slots:
  void initTestCase()
  {
    qRegisterMetaType<QProcess::ProcessError>();
    qRegisterMetaType<QProcess::ExitStatus>();
    pull("alpine");
    pull("hello-world");
  }

  void cleanupTestCase() {}

  void runTest()
  {
    auto runInvocation = docker::run("hello-world");
    QSignalSpy runError(runInvocation, &docker::DockerPullInvocation::error);
    QSignalSpy runFinished(runInvocation,
                           &docker::DockerPullInvocation::finished);

    QVERIFY(runFinished.wait(m_invocationTimeout));
    QVERIFY(runError.isEmpty());
    QCOMPARE(runFinished.size(), 1);
    auto arguments = runFinished.takeFirst();
    QCOMPARE(arguments.at(0).toInt(), 0);

    auto containerId = runInvocation->containerId();
    QVERIFY(!containerId.isEmpty());
    runInvocation->deleteLater();

    auto logInvocation = docker::logs(containerId);
    QSignalSpy logError(logInvocation, &docker::DockerLogsInvocation::error);
    QSignalSpy logFinished(logInvocation,
                           &docker::DockerLogsInvocation::finished);
    QVERIFY(logFinished.wait(m_invocationTimeout));
    QVERIFY(logError.isEmpty());
    QCOMPARE(logFinished.size(), 1);
    arguments = logFinished.takeFirst();
    QCOMPARE(arguments.at(0).toInt(), 0);
    QVERIFY(logInvocation->logs().trimmed().startsWith("Hello from Docker!"));
    logInvocation->deleteLater();
    remove(containerId);
  }

  void pullTest()
  {
    auto pullInvocation = docker::run("alpine");
    QSignalSpy pullError(pullInvocation, &docker::DockerPullInvocation::error);
    QSignalSpy pullFinished(pullInvocation,
                            &docker::DockerPullInvocation::finished);
    QVERIFY(pullFinished.wait(m_invocationTimeout));
    QVERIFY(pullError.isEmpty());
    QCOMPARE(pullFinished.size(), 1);
    auto arguments = pullFinished.takeFirst();
    QCOMPARE(arguments.at(0).toInt(), 0);
  }

  void runBindMountTest()
  {
    QMap<QString, QString> bindMounts;
    QTemporaryDir tempDir;

    bindMounts[tempDir.path()] = "/test";
    QString entryPoint = "/bin/sh";
    QStringList args;
    args.append("-c");
    args.append("echo 'world' > /test/hello.txt");

    auto runInvocation = docker::run("alpine", entryPoint, args, bindMounts);
    QSignalSpy runError(runInvocation, &docker::DockerPullInvocation::error);
    QSignalSpy runFinished(runInvocation,
                           &docker::DockerPullInvocation::finished);
    QVERIFY(runFinished.wait(m_invocationTimeout));
    QVERIFY(runError.isEmpty());
    QCOMPARE(runFinished.size(), 1);
    auto arguments = runFinished.takeFirst();
    QCOMPARE(arguments.at(0).toInt(), 0);
    auto containerId = runInvocation->containerId();
    remove(containerId);
    runInvocation->deleteLater();

    QFile file(QDir(tempDir.path()).filePath("hello.txt"));
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(QString(file.readLine()).trimmed(), QString("world"));
  }

  void dockerErrorTest()
  {
    auto runInvocation = docker::run("alpine", "/bin/bash");
    QSignalSpy runError(runInvocation, &docker::DockerPullInvocation::error);
    QSignalSpy runFinished(runInvocation,
                           &docker::DockerPullInvocation::finished);
    QVERIFY(runFinished.wait(m_invocationTimeout));
    QVERIFY(runError.isEmpty());
    QCOMPARE(runFinished.size(), 1);
    auto arguments = runFinished.takeFirst();
    QCOMPARE(arguments.at(0).toInt(), 127);
    runInvocation->deleteLater();
  }

  void stopTest()
  {
    QString entryPoint = "/bin/sh";
    QStringList args;
    args.append("-c");
    args.append("sleep 30");

    auto runInvocation = run("alpine", entryPoint, args);
    auto containerId = runInvocation->containerId();
    QVERIFY(!containerId.isEmpty());
    runInvocation->deleteLater();

    auto stopInvocation = docker::stop(containerId, 1);
    QSignalSpy stopError(stopInvocation, &docker::DockerPullInvocation::error);
    QSignalSpy stopFinished(stopInvocation,
                            &docker::DockerPullInvocation::finished);
    QVERIFY(stopFinished.wait(m_invocationTimeout));
    QVERIFY(stopError.isEmpty());
    QCOMPARE(stopFinished.size(), 1);
    auto arguments = stopFinished.takeFirst();
    QCOMPARE(arguments.at(0).toInt(), 0);
    stopInvocation->deleteLater();

    auto inspectInvocation = docker::inspect(containerId);
    QSignalSpy inspectError(inspectInvocation,
                            &docker::DockerPullInvocation::error);
    QSignalSpy inspectFinished(inspectInvocation,
                               &docker::DockerPullInvocation::finished);
    QVERIFY(inspectFinished.wait(m_invocationTimeout));
    QVERIFY(inspectError.isEmpty());
    QCOMPARE(inspectFinished.size(), 1);
    arguments = inspectFinished.takeFirst();
    QCOMPARE(arguments.at(0).toInt(), 0);
    QCOMPARE(inspectInvocation->status(), QString("exited"));
    inspectInvocation->deleteLater();
    remove(containerId);
  }

  void inspectTest()
  {
    auto runInvocation = run("alpine");
    auto containerId = runInvocation->containerId();
    QVERIFY(!containerId.isEmpty());
    runInvocation->deleteLater();

    // Sleep for a second to let the previous container cleanup
    QTest::qSleep(1000);

    auto inspectInvocation = docker::inspect(containerId);
    QSignalSpy inspectError(inspectInvocation,
                            &docker::DockerPullInvocation::error);
    QSignalSpy inspectFinished(inspectInvocation,
                               &docker::DockerPullInvocation::finished);
    QVERIFY(inspectFinished.wait(m_invocationTimeout));
    QVERIFY(inspectError.isEmpty());
    QCOMPARE(inspectFinished.size(), 1);
    auto arguments = inspectFinished.takeFirst();
    QCOMPARE(arguments.at(0).toInt(), 0);
    QCOMPARE(inspectInvocation->status(), QString("exited"));
    QCOMPARE(inspectInvocation->exitCode(), 0);
    inspectInvocation->deleteLater();
    remove(containerId);
  }

  void removeTest()
  {
    auto runInvocation = run("alpine");
    auto containerId = runInvocation->containerId();
    QVERIFY(!containerId.isEmpty());
    runInvocation->deleteLater();

    // Sleep for a second to let the previous container cleanup
    QTest::qSleep(1000);

    auto removeInvocation = docker::remove(containerId);
    QSignalSpy removeError(removeInvocation,
                           &docker::DockerRemoveInvocation::error);
    QSignalSpy removeFinished(removeInvocation,
                              &docker::DockerRemoveInvocation::finished);
    QVERIFY(removeFinished.wait(m_invocationTimeout));
    QVERIFY(removeError.isEmpty());
    QCOMPARE(removeFinished.size(), 1);
    removeInvocation->deleteLater();

    auto inspectInvocation = docker::inspect(containerId);
    QSignalSpy inspectError(inspectInvocation,
                            &docker::DockerPullInvocation::error);
    QSignalSpy inspectFinished(inspectInvocation,
                               &docker::DockerPullInvocation::finished);
    QVERIFY(inspectFinished.wait(m_invocationTimeout));
    QVERIFY(inspectError.isEmpty());
    QCOMPARE(inspectFinished.size(), 1);
    auto arguments = inspectFinished.takeFirst();
    QCOMPARE(arguments.at(0).toInt(), 1);
    inspectInvocation->deleteLater();
  }
};

QTEST_GUILESS_MAIN(DockerUtilitiesTest)
#include "DockerUtilitiesTest.moc"
