// "Remember me" against the REAL credential store of the platform (Windows Credential
// Manager, Android Keystore, Secret Service on Linux) through Session / SecureStore /
// QtKeychain, across separate processes — what a user does when closing and reopening
// the application. A local fake API answers /auth/refresh and /me; no real server.
//
// Without arguments (desktop, ctest): a Qt Test case runs each phase in a new process.
// With "--phase NAME" (also how the Android script starts the app between restarts):
// runs one phase and prints "NHILL_KEYCHAIN_PHASE NAME OK" or "... FAIL <reason>".
//   write      remember me on, sign-in (ref-1)       -> store holds ref-1
//   read       new process: restore with ref-1       -> signed in, store holds ref-2 (rotation)
//   logout     new process: restore with ref-2, logout -> store empty, restore fails
//   forget     remember me off, sign-in (ref-3)      -> nothing stored
// NHILL_KEYCHAIN_REQUIRED=1 turns "no secure store available" into a failure (CI on
// Windows and Android); otherwise the test is skipped where no keyring service runs.
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJSEngine>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QtTest>

#include <cstdio>
#include <functional>

#include "apiclient.h"
#include "session.h"

using namespace nhc;

namespace {

const QString kTestService = QStringLiteral("NhillKeychainTest");

bool waitFor(const std::function<bool()>& cond, int ms = 15000) {
    QElapsedTimer t;
    t.start();
    while (!cond()) {
        if (t.elapsed() > ms) return false;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(5);
    }
    return true;
}

// Minimal HTTP/1.1 API (one request per connection).
class FakeApi : public QObject {
public:
    QString expectedRefresh, nextAccess = QStringLiteral("acc-2"), nextRefresh = QStringLiteral("ref-2");
    QStringList presentedRefresh;
    int meHits = 0;
    quint16 port() const { return m_server.serverPort(); }
    FakeApi() {
        m_server.listen(QHostAddress::LocalHost, 0);
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket* s = m_server.nextPendingConnection()) {
                connect(s, &QTcpSocket::readyRead, this, [this, s] { onData(s); });
                connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
            }
        });
    }

private:
    void onData(QTcpSocket* s) {
        QByteArray& buf = m_buffers[s];
        buf += s->readAll();
        int end = buf.indexOf("\r\n\r\n");
        if (end < 0) return;
        const QByteArray head = buf.left(end);
        int length = 0;
        for (const QByteArray& line : head.split('\n'))
            if (line.toLower().startsWith("content-length:")) length = line.mid(15).trimmed().toInt();
        if (buf.size() < end + 4 + length) return;
        const QByteArray body = buf.mid(end + 4, length);
        const QList<QByteArray> first = head.left(head.indexOf("\r\n")).split(' ');
        const QByteArray method = first.value(0), path = first.value(1);
        m_buffers.remove(s);
        if (method == "POST" && path == "/api/v1/auth/refresh") {
            const QString presented = QJsonDocument::fromJson(body).object().value(QStringLiteral("refresh_token")).toString();
            presentedRefresh << presented;
            if (presented != expectedRefresh)
                return reply(s, 401, R"({"error":{"code":"invalid_refresh_token","message":"x"}})");
            return reply(s, 200, QJsonDocument(QJsonObject{{QStringLiteral("access_token"), nextAccess},
                                                           {QStringLiteral("refresh_token"), nextRefresh},
                                                           {QStringLiteral("token_type"), QStringLiteral("Bearer")}})
                                     .toJson(QJsonDocument::Compact));
        }
        if (method == "GET" && path == "/api/v1/me") {
            ++meHits;
            return reply(s, 200, R"({"user":{"id":"u1","email":"owner@example.cd","full_name":"Test"},"role":"owner",)"
                                 R"("permissions":[],"establishment":{"id":"e1","trade_name":"Test","kind":"company",)"
                                 R"("main_currency":"CDF"},"subscription":{"status":"active","write_allowed":true}})");
        }
        if (method == "POST" && path == "/api/v1/auth/logout") return reply(s, 204, "");
        reply(s, 404, R"({"error":{"code":"not_found","message":"x"}})");
    }
    void reply(QTcpSocket* s, int status, const QByteArray& body) {
        QByteArray r = "HTTP/1.1 " + QByteArray::number(status) + (status < 300 ? " OK" : " ERR") +
                       "\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: " +
                       QByteArray::number(body.size()) + "\r\n\r\n" + body;
        s->write(r);
        s->flush();
        s->disconnectFromHost();
    }
    QTcpServer m_server;
    QHash<QTcpSocket*, QByteArray> m_buffers;
};

struct Rig {
    FakeApi api;
    ApiClient client;
    SecureStore store{nullptr, kTestService};
    Session session{&client, &store};
    Rig() { client.setBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(api.port())); }
    QString key() const { return QStringLiteral("refresh@127.0.0.1"); }
    // Value in the store: {found, value}
    QPair<bool, QString> stored() {
        bool done = false, found = false;
        QString v;
        store.read(key(), [&](bool ok, QString value) {
            found = ok;
            v = value;
            done = true;
        });
        waitFor([&] { return done; });
        return {found, v};
    }
    // Session::restore() as the QML splash screen calls it; returns the callback's verdict.
    QJSEngine engine;
    bool restore() {
        engine.evaluate(QStringLiteral("var finished = false, result = false;"));
        QJSValue cb = engine.evaluate(QStringLiteral("(function(ok) { finished = true; result = ok; })"));
        session.restore(cb);
        waitFor([&] { return engine.globalObject().property(QStringLiteral("finished")).toBool(); }, 20000);
        return engine.globalObject().property(QStringLiteral("result")).toBool() && session.loggedIn();
    }
};

void configureSettings() {
    QCoreApplication::setOrganizationName(QStringLiteral("NhillTests"));
    QCoreApplication::setApplicationName(QStringLiteral("KeychainTest"));
}

QString runPhase(const QString& phase) {
    if (!SecureStore::supported()) return QStringLiteral("SKIP built without QtKeychain");
    Rig rig;
    if (phase == QLatin1String("write")) {
        rig.store.remove(rig.key());
        rig.session.setRemember(true);
        rig.session.start({{QStringLiteral("access_token"), QStringLiteral("acc-1")},
                           {QStringLiteral("refresh_token"), QStringLiteral("ref-1")}});
        if (!waitFor([&] { return rig.session.loggedIn(); })) return QStringLiteral("FAIL no session after start");
        waitFor([&] { return rig.stored().second == QLatin1String("ref-1") || !rig.store.available(); }, 10000);
        if (!rig.store.available()) return QStringLiteral("UNAVAILABLE ") + rig.store.lastError();
        if (rig.stored().second != QLatin1String("ref-1")) return QStringLiteral("FAIL refresh token not stored");
        return QStringLiteral("OK");
    }
    if (phase == QLatin1String("read")) {
        if (!rig.session.remember()) return QStringLiteral("FAIL remember-me preference lost");
        rig.api.expectedRefresh = QStringLiteral("ref-1");
        if (!rig.restore()) return QStringLiteral("FAIL session not restored (presented: %1)").arg(rig.api.presentedRefresh.join(','));
        if (!waitFor([&] { return rig.stored().second == QLatin1String("ref-2"); }, 10000))
            return QStringLiteral("FAIL rotated refresh token not stored");
        return QStringLiteral("OK");
    }
    if (phase == QLatin1String("logout")) {
        rig.api.expectedRefresh = QStringLiteral("ref-2");
        rig.api.nextRefresh = QStringLiteral("ref-9");
        if (!rig.restore()) return QStringLiteral("FAIL session not restored before logout");
        waitFor([&] { return rig.stored().second == QLatin1String("ref-9"); }, 10000);
        rig.session.logout();
        if (!waitFor([&] { return !rig.stored().first; }, 10000)) return QStringLiteral("FAIL token kept after logout");
        Rig again;
        again.api.expectedRefresh = QStringLiteral("ref-9");
        if (again.restore()) return QStringLiteral("FAIL session restored after logout");
        return QStringLiteral("OK");
    }
    if (phase == QLatin1String("forget")) {
        rig.session.setRemember(true);
        rig.session.start({{QStringLiteral("access_token"), QStringLiteral("acc-3")},
                           {QStringLiteral("refresh_token"), QStringLiteral("ref-3")}});
        waitFor([&] { return rig.stored().second == QLatin1String("ref-3"); }, 10000);
        rig.session.setRemember(false);  // unticked: what was stored is erased now
        if (!waitFor([&] { return !rig.stored().first; }, 10000)) return QStringLiteral("FAIL token kept after unticking");
        rig.session.start({{QStringLiteral("access_token"), QStringLiteral("acc-4")},
                           {QStringLiteral("refresh_token"), QStringLiteral("ref-4")}});
        waitFor([&] { return rig.session.loggedIn(); });
        QThread::msleep(300);
        if (rig.stored().first) return QStringLiteral("FAIL token stored without remember me");
        Rig next;
        next.api.expectedRefresh = QStringLiteral("ref-4");
        if (next.restore()) return QStringLiteral("FAIL session restored without remember me");
        return QStringLiteral("OK");
    }
    return QStringLiteral("FAIL unknown phase");
}

bool required() { return qEnvironmentVariable("NHILL_KEYCHAIN_REQUIRED") == QLatin1String("1"); }

}  // namespace

class KeychainTest : public QObject {
    Q_OBJECT
private:
    void phase(const QString& name) {
        QProcess p;
        p.setProcessChannelMode(QProcess::MergedChannels);
        p.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--phase"), name});
        QVERIFY2(p.waitForFinished(60000), "phase process timed out");
        const QString out = QString::fromLocal8Bit(p.readAll());
        const QString marker = QStringLiteral("NHILL_KEYCHAIN_PHASE %1 ").arg(name);
        const int at = out.indexOf(marker);
        const QString verdict = at < 0 ? QStringLiteral("FAIL no verdict: ") + out
                                       : out.mid(at + marker.size()).section(QLatin1Char('\n'), 0, 0).trimmed();
        if (verdict.startsWith(QLatin1String("SKIP")) || verdict.startsWith(QLatin1String("UNAVAILABLE"))) {
            if (required()) QFAIL(qPrintable(QStringLiteral("secure store required but ") + verdict));
            QSKIP(qPrintable(verdict));
        }
        QVERIFY2(verdict == QLatin1String("OK"), qPrintable(name + QStringLiteral(": ") + verdict + QStringLiteral("\n") + out));
    }
private slots:
    void sessionSurvivesARestartWhenRemembered() {
        phase(QStringLiteral("write"));
        phase(QStringLiteral("read"));
    }
    void logoutErasesTheStoredSession() { phase(QStringLiteral("logout")); }
    void nothingIsStoredWithoutRememberMe() { phase(QStringLiteral("forget")); }
};

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    configureSettings();
    QStringList args = app.arguments();
    // Android: the arguments come from the "applicationArguments" extra of the intent; if the
    // launcher cannot pass them, each start of the app runs the next phase in order.
    int i = args.indexOf(QStringLiteral("--phase"));
    QString name = i >= 0 && i + 1 < args.size() ? args.at(i + 1) : QString();
#ifdef Q_OS_ANDROID
    if (name.isEmpty()) {
        static const QStringList order = {QStringLiteral("write"), QStringLiteral("read"), QStringLiteral("logout"),
                                          QStringLiteral("forget")};
        QSettings st;
        const int n = st.value(QStringLiteral("test/next"), 0).toInt();
        name = order.value(n, QStringLiteral("done"));
        st.setValue(QStringLiteral("test/next"), n + 1);
        st.sync();
    }
#endif
    if (!name.isEmpty()) {
        const QString verdict = runPhase(name);
        const QByteArray line = "NHILL_KEYCHAIN_PHASE " + name.toUtf8() + " " + verdict.toUtf8();
        std::printf("%s\n", line.constData());
        std::fflush(stdout);
        qInfo("%s", line.constData());  // logcat on Android
        return verdict == QLatin1String("OK") || verdict.startsWith(QLatin1String("SKIP")) ? 0 : 1;
    }
    KeychainTest t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_keychain.moc"
