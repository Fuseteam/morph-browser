/*
 * Copyright 2015-2016 Canonical Ltd.
 *
 * This file is part of webbrowser-app.
 *
 * webbrowser-app is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * webbrowser-app is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTemporaryFile>
#include <QtTest/QSignalSpy>
#include <QtTest/QtTest>
#include "downloads-model.h"

class DownloadsModelTests : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir homeDir;
    DownloadsModel* model;

private Q_SLOTS:
    void init()
    {
        // QStandardPaths::setTestModeEnabled() doesn't affect
        // QStandardPaths::DownloadLocation, so we must override $HOME to
        // ensure the test won't write data to the user's home directory.
        qputenv("HOME", homeDir.path().toUtf8());
        model = new DownloadsModel;
        model->setDatabasePath(":memory:");
    }

    void cleanup()
    {
        delete model;
        qunsetenv("HOME");
    }

    void shouldBeInitiallyEmpty()
    {
        QCOMPARE(model->rowCount(), 0);
    }

    void shouldExposeRoleNames()
    {
        QList<QByteArray> roleNames = model->roleNames().values();
        QVERIFY(roleNames.contains("downloadId"));
        QVERIFY(roleNames.contains("url"));
        QVERIFY(roleNames.contains("path"));
        QVERIFY(roleNames.contains("filename"));
        QVERIFY(roleNames.contains("mimetype"));
        QVERIFY(roleNames.contains("complete"));
        QVERIFY(roleNames.contains("paused"));
        QVERIFY(roleNames.contains("error"));
        QVERIFY(roleNames.contains("created"));
        QVERIFY(roleNames.contains("incognito"));
    }

    void shouldContainAddedEntries()
    {
        QVERIFY(!model->contains(QStringLiteral("testid")));
        model->add(QStringLiteral("testid"), QUrl(QStringLiteral("http://example.org/")), QStringLiteral("examplepath"), QStringLiteral("text/html"), false);
        QVERIFY(model->contains(QStringLiteral("testid")));
    }

    void shouldAddNewEntries()
    {
        QSignalSpy spy(model, SIGNAL(rowsInserted(const QModelIndex&, int, int)));

        model->add(QStringLiteral("testid"), QUrl(QStringLiteral("http://example.org/")), QStringLiteral("examplepath"), QStringLiteral("text/plain"), false);
        QCOMPARE(model->rowCount(), 1);
        QCOMPARE(spy.count(), 1);
        QVariantList args = spy.takeFirst();
        QCOMPARE(args.at(0).toInt(), 0);
        QCOMPARE(args.at(1).toInt(), 0);
        QCOMPARE(model->data(model->index(0), DownloadsModel::DownloadId).toString(), QStringLiteral("testid"));
        QCOMPARE(model->data(model->index(0), DownloadsModel::Url).toUrl(), QUrl(QStringLiteral("http://example.org/")));
        QCOMPARE(model->data(model->index(0), DownloadsModel::Path).toString(), QStringLiteral("examplepath"));
        QCOMPARE(model->data(model->index(0), DownloadsModel::Mimetype).toString(), QStringLiteral("text/plain"));

        model->add(QStringLiteral("testid2"), QUrl(QStringLiteral("http://example.org/pdf")), QStringLiteral("examplepath2"), QStringLiteral("application/pdf"), false);
        QCOMPARE(model->rowCount(), 2);
        QCOMPARE(spy.count(), 1);
        args = spy.takeFirst();
        QCOMPARE(args.at(0).toInt(), 0);
        QCOMPARE(args.at(1).toInt(), 0);
        QCOMPARE(model->data(model->index(0), DownloadsModel::DownloadId).toString(), QStringLiteral("testid2"));
        QCOMPARE(model->data(model->index(0), DownloadsModel::Url).toUrl(), QUrl(QStringLiteral("http://example.org/pdf")));
        QCOMPARE(model->data(model->index(0), DownloadsModel::Path).toString(), QStringLiteral("examplepath2"));
        QCOMPARE(model->data(model->index(0), DownloadsModel::Mimetype).toString(), QStringLiteral("application/pdf"));
    }

    void shouldRemoveCancelled()
    {
        model->add(QStringLiteral("testid"), QUrl(QStringLiteral("http://example.org/")), QStringLiteral("examplepath"), QStringLiteral("text/plain"), false);
        model->add(QStringLiteral("testid2"), QUrl(QStringLiteral("http://example.org/pdf")), QStringLiteral("examplepath2"), QStringLiteral("application/pdf"), false);
        model->add(QStringLiteral("testid3"), QUrl(QStringLiteral("https://example.org/secure.png")), QStringLiteral("examplepath3"), QStringLiteral("image/png"), false);
        QCOMPARE(model->rowCount(), 3);

        model->cancelDownload(QStringLiteral("testid2"));
        QCOMPARE(model->rowCount(), 2);

        model->cancelDownload(QStringLiteral("invalid"));
        QCOMPARE(model->rowCount(), 2);
    }

    void shouldCompleteDownloads()
    {
        model->add(QStringLiteral("testid"), QUrl(QStringLiteral("http://example.org/")), QStringLiteral("examplepath"), QStringLiteral("text/plain"), false);
        QVERIFY(!model->data(model->index(0, 0), DownloadsModel::Complete).toBool());
        QSignalSpy spy(model, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&, const QVector<int>&)));

        model->setComplete(QStringLiteral("testid"), true);
        QVERIFY(model->data(model->index(0, 0), DownloadsModel::Complete).toBool());
        QCOMPARE(spy.count(), 1);
        QVariantList args = spy.takeFirst();
        QCOMPARE(args.at(0).toModelIndex().row(), 0);
        QCOMPARE(args.at(1).toModelIndex().row(), 0);
        QVector<int> roles = args.at(2).value<QVector<int> >();
        QCOMPARE(roles.size(), 1);
        QCOMPARE(roles.at(0), (int) DownloadsModel::Complete);

        model->setComplete(QStringLiteral("testid"), true);
        QVERIFY(model->data(model->index(0, 0), DownloadsModel::Complete).toBool());
        QVERIFY(spy.isEmpty());

        model->setComplete(QStringLiteral("testid"), false);
        QVERIFY(!model->data(model->index(0, 0), DownloadsModel::Complete).toBool());
        QCOMPARE(spy.count(), 1);
        args = spy.takeFirst();
        QCOMPARE(args.at(0).toModelIndex().row(), 0);
        QCOMPARE(args.at(1).toModelIndex().row(), 0);
        roles = args.at(2).value<QVector<int> >();
        QCOMPARE(roles.size(), 1);
        QCOMPARE(roles.at(0), (int) DownloadsModel::Complete);
    }

    void shouldSetError()
    {
        model->add(QStringLiteral("testid"), QUrl(QStringLiteral("http://example.org/")), QStringLiteral("examplepath"), QStringLiteral("text/plain"), false);
        QVERIFY(model->data(model->index(0, 0), DownloadsModel::Error).toString().isEmpty());
        QSignalSpy spy(model, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&, const QVector<int>&)));

        model->setError(QStringLiteral("testid"), QStringLiteral("foo"));
        QCOMPARE(model->data(model->index(0, 0), DownloadsModel::Error).toString(), QStringLiteral("foo"));
        QCOMPARE(spy.count(), 1);
        QVariantList args = spy.takeFirst();
        QCOMPARE(args.at(0).toModelIndex().row(), 0);
        QCOMPARE(args.at(1).toModelIndex().row(), 0);
        QVector<int> roles = args.at(2).value<QVector<int> >();
        QCOMPARE(roles.size(), 1);
        QCOMPARE(roles.at(0), (int) DownloadsModel::Error);

        model->setError(QStringLiteral("testid"), QStringLiteral("foo"));
        QCOMPARE(model->data(model->index(0, 0), DownloadsModel::Error).toString(), QStringLiteral("foo"));
        QVERIFY(spy.isEmpty());

        model->setError(QStringLiteral("testid"), QString("bar"));
        QCOMPARE(model->data(model->index(0, 0), DownloadsModel::Error).toString(), QStringLiteral("bar"));
        QCOMPARE(spy.count(), 1);
        args = spy.takeFirst();
        QCOMPARE(args.at(0).toModelIndex().row(), 0);
        QCOMPARE(args.at(1).toModelIndex().row(), 0);
        roles = args.at(2).value<QVector<int> >();
        QCOMPARE(roles.size(), 1);
        QCOMPARE(roles.at(0), (int) DownloadsModel::Error);
    }

    void shouldPauseAndResumeDownload()
    {
        model->add(QStringLiteral("testid"), QUrl(QStringLiteral("http://example.org/")), QStringLiteral("examplepath"), QStringLiteral("text/plain"), false);
        QVERIFY(!model->data(model->index(0, 0), DownloadsModel::Paused).toBool());
        QSignalSpy spy(model, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&, const QVector<int>&)));

        model->pauseDownload(QStringLiteral("testid"));
        QVERIFY(model->data(model->index(0, 0), DownloadsModel::Paused).toBool());
        QCOMPARE(spy.count(), 1);
        QVariantList args = spy.takeFirst();
        QCOMPARE(args.at(0).toModelIndex().row(), 0);
        QCOMPARE(args.at(1).toModelIndex().row(), 0);
        QVector<int> roles = args.at(2).value<QVector<int> >();
        QCOMPARE(roles.size(), 1);
        QCOMPARE(roles.at(0), (int) DownloadsModel::Paused);

        model->pauseDownload(QStringLiteral("testid"));
        QVERIFY(model->data(model->index(0, 0), DownloadsModel::Paused).toBool());
        QVERIFY(spy.isEmpty());

        model->resumeDownload(QStringLiteral("testid"));
        QVERIFY(!model->data(model->index(0, 0), DownloadsModel::Paused).toBool());
        QCOMPARE(spy.count(), 1);
        args = spy.takeFirst();
        QCOMPARE(args.at(0).toModelIndex().row(), 0);
        QCOMPARE(args.at(1).toModelIndex().row(), 0);
        roles = args.at(2).value<QVector<int> >();
        QCOMPARE(roles.size(), 1);
        QCOMPARE(roles.at(0), (int) DownloadsModel::Paused);
    }

    void shouldKeepEntriesSortedChronologically()
    {
        model->add(QStringLiteral("testid"), QUrl(QStringLiteral("http://example.org/")), QStringLiteral("examplepath"), QStringLiteral("text/plain"), false);
        model->add(QStringLiteral("testid2"), QUrl(QStringLiteral("http://example.org/pdf")), QStringLiteral("examplepath2"), QStringLiteral("application/pdf"), false);
        model->add(QStringLiteral("testid3"), QUrl(QStringLiteral("https://example.org/secure.png")), QStringLiteral("examplepath3"), QStringLiteral("image/png"), false);

        QCOMPARE(model->data(model->index(0, 0), DownloadsModel::DownloadId).toString(), QStringLiteral("testid3"));
        QCOMPARE(model->data(model->index(1, 0), DownloadsModel::DownloadId).toString(), QStringLiteral("testid2"));
        QCOMPARE(model->data(model->index(2, 0), DownloadsModel::DownloadId).toString(), QStringLiteral("testid"));
    }

    void shouldReturnData()
    {
        model->add(QStringLiteral("testid"), QUrl(QStringLiteral("http://example.org/")), QStringLiteral("examplepath"), QStringLiteral("text/plain"), false);
        QVERIFY(!model->data(QModelIndex(), DownloadsModel::DownloadId).isValid());
        QVERIFY(!model->data(model->index(-1, 0), DownloadsModel::DownloadId).isValid());
        QVERIFY(!model->data(model->index(3, 0), DownloadsModel::DownloadId).isValid());
        QCOMPARE(model->data(model->index(0, 0), DownloadsModel::DownloadId).toString(), QStringLiteral("testid"));
        QCOMPARE(model->data(model->index(0, 0), DownloadsModel::Url).toUrl(), QUrl(QStringLiteral("http://example.org/")));
        QCOMPARE(model->data(model->index(0, 0), DownloadsModel::Mimetype).toString(), QStringLiteral("text/plain"));
        QVERIFY(model->data(model->index(0, 0), DownloadsModel::Created).toDateTime() <= QDateTime::currentDateTime());
        QVERIFY(!model->data(model->index(0, 0), DownloadsModel::Complete).toBool());
        QVERIFY(!model->data(model->index(0, 0), DownloadsModel::Incognito).toBool());
        QVERIFY(!model->data(model->index(0, 0), -1).isValid());
    }

    void shouldReturnDatabasePath()
    {
        QCOMPARE(model->databasePath(), QStringLiteral(":memory:"));
    }

    void shouldNotifyWhenSettingDatabasePath()
    {
        QSignalSpy spyPath(model, SIGNAL(databasePathChanged()));
        QSignalSpy spyReset(model, SIGNAL(modelReset()));

        model->setDatabasePath(QStringLiteral(":memory:"));
        QVERIFY(spyPath.isEmpty());
        QVERIFY(spyReset.isEmpty());

        model->setDatabasePath(QStringLiteral(""));
        QCOMPARE(spyPath.count(), 1);
        QCOMPARE(spyReset.count(), 1);
        QCOMPARE(model->databasePath(), QStringLiteral(":memory:"));
    }

    void shouldSerializeOnDisk()
    {
        QTemporaryFile tempFile;
        tempFile.open();
        QString fileName = tempFile.fileName();
        delete model;
        model = new DownloadsModel;
        model->setDatabasePath(fileName);
        model->add(QStringLiteral("testid"), QUrl(QStringLiteral("http://example.org/")), QStringLiteral("examplepath"), QStringLiteral("text/plain"), false);
        model->add(QStringLiteral("testid2"), QUrl(QStringLiteral("http://example.org/pdf")), QStringLiteral("examplepath2"), QStringLiteral("application/pdf"), false);
        model->add(QStringLiteral("testid3"), QUrl(QStringLiteral("http://example.org/incognito.pdf")), QStringLiteral("examplepath3"), QStringLiteral("application/pdf"), true);
        QCOMPARE(model->rowCount(), 3);
        delete model;
        model = new DownloadsModel;
        model->setDatabasePath(fileName);
        model->fetchMore();
        QCOMPARE(model->rowCount(), 2);
    }

    void shouldCountNumberOfEntries()
    {
        QCOMPARE(model->property("count").toInt(), 0);
        QCOMPARE(model->rowCount(), 0);
        model->add(QStringLiteral("testid"), QUrl(QStringLiteral("http://example.org/")), QStringLiteral("examplepath"), QStringLiteral("text/plain"), false);
        QCOMPARE(model->property("count").toInt(), 1);
        QCOMPARE(model->rowCount(), 1);
        model->add(QStringLiteral("testid2"), QUrl(QStringLiteral("http://example.org/pdf")), QStringLiteral("examplepath2"), QStringLiteral("application/pdf"), false);
        QCOMPARE(model->property("count").toInt(), 2);
        QCOMPARE(model->rowCount(), 2);
        model->add(QStringLiteral("testid3"), QUrl(QStringLiteral("https://example.org/secure.png")), QStringLiteral("examplepath3"), QStringLiteral("image/png"), false);
        QCOMPARE(model->property("count").toInt(), 3);
        QCOMPARE(model->rowCount(), 3);
    }

    void shouldPruneIncognitoDownloads()
    {
        model->add(QStringLiteral("testid1"), QUrl(QStringLiteral("http://example.org/1")), QStringLiteral("examplepath1"), QStringLiteral("text/plain"), false);
        model->add(QStringLiteral("testid2"), QUrl(QStringLiteral("http://example.org/2")), QStringLiteral("examplepath2"), QStringLiteral("text/plain"), true);
        model->add(QStringLiteral("testid3"), QUrl(QStringLiteral("http://example.org/3")), QStringLiteral("examplepath3"), QStringLiteral("text/plain"), false);
        model->add(QStringLiteral("testid4"), QUrl(QStringLiteral("http://example.org/4")), QStringLiteral("examplepath4"), QStringLiteral("text/plain"), true);
        QCOMPARE(model->rowCount(), 4);
        QSignalSpy spyRowsRemoved(model, SIGNAL(rowsRemoved(const QModelIndex&, int, int)));
        QSignalSpy spyRowCountChanged(model, SIGNAL(rowCountChanged()));
        model->pruneIncognitoDownloads();
        QCOMPARE(model->rowCount(), 2);
        QCOMPARE(model->data(model->index(0), DownloadsModel::DownloadId).toString(), QStringLiteral("testid3"));
        QCOMPARE(model->data(model->index(1), DownloadsModel::DownloadId).toString(), QStringLiteral("testid1"));
        QCOMPARE(spyRowsRemoved.count(), 2);
        QCOMPARE(spyRowCountChanged.count(), 2);
    }

    void shouldFailToMoveInvalidDownload()
    {
        model->add(QStringLiteral("testid"), QUrl(QStringLiteral("http://example.org/")), QStringLiteral("examplepath"), QStringLiteral("text/plain"), false);
        QTemporaryFile tempFile;
        tempFile.open();
        QSignalSpy spy(model, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&, const QVector<int>&)));
        model->moveToDownloads(QStringLiteral("foobar"), tempFile.fileName());
        QVERIFY(spy.isEmpty());
    }

    void shouldFailToMoveNonExistentFile()
    {
        model->add(QStringLiteral("testid"), QUrl(QStringLiteral("http://example.org/")), QStringLiteral("examplepath"), QStringLiteral("text/plain"), false);
        QTemporaryFile tempFile;
        tempFile.open();
        QString fileName = tempFile.fileName();
        tempFile.remove();
        QSignalSpy spy(model, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&, const QVector<int>&)));
        QTest::ignoreMessage(QtWarningMsg, QString("Download not found: \"%1\"").arg(fileName).toUtf8().constData());
        model->moveToDownloads(QStringLiteral("testid"), fileName);
        QVERIFY(spy.isEmpty());
    }

    void shouldMoveFile()
    {
        model->add(QStringLiteral("testid"), QUrl(QStringLiteral("http://example.org/")), QStringLiteral("examplepath"), QStringLiteral("application/pdf"), false);
        QTemporaryFile tempFile(QStringLiteral("XXXXXX.txt"));
        tempFile.open();
        tempFile.write(QByteArray("foo bar baz"));
        tempFile.close();
        QString filePath = tempFile.fileName();
        QString fileName = QFileInfo(filePath).fileName();
        QVERIFY(QFile::exists(filePath));
        QSignalSpy spy(model, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&, const QVector<int>&)));
        model->moveToDownloads(QStringLiteral("testid"), filePath);
        QCOMPARE(spy.count(), 1);
        QVariantList args = spy.takeFirst();
        QCOMPARE(args.at(0).toModelIndex().row(), 0);
        QCOMPARE(args.at(1).toModelIndex().row(), 0);
        QVector<int> roles = args.at(2).value<QVector<int> >();
        QCOMPARE(roles.size(), 2);
        QVERIFY(roles.contains(DownloadsModel::Mimetype));
        QVERIFY(roles.contains(DownloadsModel::Path));
        QCOMPARE(model->data(model->index(0), DownloadsModel::Mimetype).toString(), QStringLiteral("text/plain"));
        QCOMPARE(model->data(model->index(0), DownloadsModel::Path).toString(), QString("%1/Downloads/%2").arg(homeDir.path(), fileName));
        QVERIFY(!QFile::exists(filePath));
    }

    void shouldRenameFileToAvoidFilenameCollision()
    {
        model->add(QStringLiteral("testid"), QUrl(QStringLiteral("http://example.org/")), QStringLiteral("examplepath"), QStringLiteral("text/plain"), false);
        QTemporaryFile tempFile(QStringLiteral("XXXXXX.txt"));
        tempFile.open();
        tempFile.write(QByteArray("foo"));
        tempFile.close();
        QString filePath = tempFile.fileName();
        QString fileName = QFileInfo(filePath).fileName();
        QVERIFY(QFile::exists(filePath));
        QSignalSpy spy(model, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&, const QVector<int>&)));
        QString path = QString("%1/Downloads/%2").arg(homeDir.path(), fileName);
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write("bar") != -1);
        file.close();
        model->moveToDownloads(QStringLiteral("testid"), filePath);
        QString otherPath = QString("%1/Downloads/%2").arg(homeDir.path(), fileName.replace(QStringLiteral("."), QStringLiteral(".1.")));
        QCOMPARE(model->data(model->index(0), DownloadsModel::Path).toString(), otherPath);
        QVERIFY(!QFile::exists(filePath));
        QVERIFY(QFile::exists(path));
        QVERIFY(QFile::exists(otherPath));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("bar"));
        file.close();
        QFile file2(otherPath);
        QVERIFY(file2.open(QIODevice::ReadOnly));
        QCOMPARE(file2.readAll(), QByteArray("foo"));
        file2.close();
    }

    void shouldDeleteDownload()
    {
        // Need a file saved on disk to allow deleting it
        model->add(QStringLiteral("testid"), QUrl(QStringLiteral("http://example.org/")), QStringLiteral("examplepath"), QStringLiteral("text/plain"), false);
        QTemporaryFile tempFile(QStringLiteral("XXXXXX.txt"));
        tempFile.open();
        tempFile.write(QByteArray("foo bar baz"));
        tempFile.close();
        QString filePath = tempFile.fileName();
        QString fileName = QFileInfo(filePath).fileName();
        model->moveToDownloads(QStringLiteral("testid"), filePath);
        QString path = model->data(model->index(0), DownloadsModel::Path).toString();
        QVERIFY(QFile::exists(path));

        QSignalSpy spyRowsRemoved(model, SIGNAL(rowsRemoved(const QModelIndex&, int, int)));
        QSignalSpy spyRowCount(model, SIGNAL(rowCountChanged()));
        model->deleteDownload(path);
        QCOMPARE(spyRowsRemoved.count(), 1);
        QVariantList args = spyRowsRemoved.takeFirst();
        QVERIFY(!args.at(0).toModelIndex().isValid());
        QCOMPARE(args.at(1).toInt(), 0);
        QCOMPARE(args.at(2).toInt(), 0);
        QCOMPARE(spyRowCount.count(), 1);
        QCOMPARE(model->rowCount(), 0);
        QVERIFY(!QFile::exists(path));
    }
};

QTEST_MAIN(DownloadsModelTests)
#include "tst_DownloadsModelTests.moc"
