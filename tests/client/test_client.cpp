// Client core tests (Qt Test): formatting, validation and the network rules of
// ApiClient (JS bodies, idempotency keys, no automatic replay of writes,
// HTTPS-only, single-flight token refresh). A local cpp-httplib server plays
// the API; nothing here talks to a real Nhill server.
#include <QJSEngine>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QtTest>

#include <atomic>
#include <mutex>
#include <thread>

#include "apiclient.h"
#include "formatter.h"

#ifdef NHILL_TEST_HTTPLIB
#include <httplib.h>
#endif

using namespace nhc;

class ClientTest : public QObject {
    Q_OBJECT

private slots:
    // ------------------------------------------------------------ Formatter
    void moneyFormatting() {
        Formatter f;
        f.setLanguage(QStringLiteral("fr"));
        QCOMPARE(f.money(QStringLiteral("1234567.50"), QStringLiteral("CDF")), QStringLiteral("1\u202F234\u202F567,50\u00A0CDF"));
        f.setLanguage(QStringLiteral("en"));
        QCOMPARE(f.money(QStringLiteral("1234567.50"), QStringLiteral("USD")), QStringLiteral("USD\u00A01,234,567.50"));
    }
    void moneyParsing() {
        Formatter f;
        QCOMPARE(f.parseMoney(QStringLiteral("1 234,5"), QStringLiteral("CDF")), QStringLiteral("1234.50"));
        QCOMPARE(f.parseMoney(QStringLiteral("12.345"), QStringLiteral("USD")), QString());  // 3 decimals refused
        QCOMPARE(f.parseMoney(QStringLiteral("abc"), QStringLiteral("USD")), QString());
        QCOMPARE(f.parseQuantity(QStringLiteral("2,5")), QStringLiteral("2.5"));
    }
    void exactSums() {
        Formatter f;
        // 0.1 + 0.2 must be exactly 0.30 (no binary floating point anywhere).
        QCOMPARE(f.sum({QStringLiteral("0.10"), QStringLiteral("0.20")}, QStringLiteral("USD")), QStringLiteral("0.30"));
        QCOMPARE(f.sum({QStringLiteral("100.00"), QStringLiteral("-40.25")}, QStringLiteral("USD")), QStringLiteral("59.75"));
        QVERIFY(f.isZero(QStringLiteral("0.00")));
    }
    void lineTotals() {
        Formatter f;
        auto t = f.lineTotals(QStringLiteral("3"), QStringLiteral("18500.00"), 1000, 1600, QStringLiteral("CDF"));
        QVERIFY(t.value(QStringLiteral("valid")).toBool());
        QCOMPARE(t.value(QStringLiteral("net")).toString(), QStringLiteral("49950.00"));
        QCOMPARE(t.value(QStringLiteral("tax")).toString(), QStringLiteral("7992.00"));
    }
    void percents() {
        Formatter f;
        QCOMPARE(f.parsePercent(QStringLiteral("16")), 1600);
        QCOMPARE(f.parsePercent(QStringLiteral("12,5")), 1250);
        QCOMPARE(f.parsePercent(QStringLiteral("101")), -1);
    }

    // ------------------------------------------------------------ Validator
    void validation() {
        Validator v;
        QVERIFY(v.email(QStringLiteral("a.b@example.cd")));
        QVERIFY(!v.email(QStringLiteral("a.b@")));
        QVERIFY(v.phone(QStringLiteral("+243 81 234 5678")));
        QVERIFY(!v.password(QStringLiteral("short"), QString()).isEmpty());
        QVERIFY(v.password(QStringLiteral("Kinshasa-Gombe-2026"), QStringLiteral("x@y.cd")).isEmpty());
        QVERIFY(v.establishmentCode(QStringLiteral("nh-7y1c-dg23-affk")));
        QVERIFY(!v.establishmentCode(QStringLiteral("NH-7Y1C")));
        QCOMPARE(v.formatCode(QStringLiteral("7y1cdg23affk")), QStringLiteral("NH-7Y1C-DG23-AFFK"));
    }

    // ------------------------------------------------------------ ApiClient
    void refusesPlainHttpToRemoteHosts() {
        ApiClient api;
        api.setBaseUrl(QStringLiteral("http://api.example.com"));
        ApiResult got;
        bool done = false;
        ApiClient::Request r;
        r.path = QStringLiteral("/health");
        api.request(r, [&](const ApiResult& res) { got = res; done = true; });
        QVERIFY(done);
        QCOMPARE(got.errorCode, QStringLiteral("insecure_url"));
    }

    // The API address typed by a user keeps only its origin; no scheme means HTTPS.
    void serverAddressIsNormalised() {
        ApiClient api;
        QCOMPARE(api.normaliseServerUrl(QStringLiteral("  api.example.com/ ")), QStringLiteral("https://api.example.com"));
        QCOMPARE(api.normaliseServerUrl(QStringLiteral("HTTPS://API.Example.com:8443/api/v1")), QStringLiteral("https://api.example.com:8443"));
        QCOMPARE(api.normaliseServerUrl(QStringLiteral("http://127.0.0.1:8080")), QStringLiteral("http://127.0.0.1:8080"));
        QCOMPARE(api.normaliseServerUrl(QString()), QString());
        QCOMPARE(api.normaliseServerUrl(QStringLiteral("https://")), QString());
    }

    // A remote server in clear HTTP is refused before any request.
    void probeRefusesPlainHttp() {
        QQmlEngine engine;
        ApiClient api;
        QJSEngine::setObjectOwnership(&api, QJSEngine::CppOwnership);
        engine.globalObject().setProperty(QStringLiteral("Api"), engine.newQObject(&api));
        engine.evaluate(QStringLiteral("var got = null; Api.probeServer('http://api.example.com', function(r) { got = r })"));
        QTRY_VERIFY_WITH_TIMEOUT(engine.evaluate(QStringLiteral("got !== null")).toBool(), 2000);
        QCOMPARE(engine.evaluate(QStringLiteral("got.error.code")).toString(), QStringLiteral("insecure_url"));
    }

#ifdef NHILL_TEST_HTTPLIB
    // The address is saved only when it answers as a Nhill server; no token is sent.
    void probeChecksTheServerIdentity() {
        QQmlEngine engine;
        ApiClient api;
        api.setTokens(QStringLiteral("secret-access"), QStringLiteral("secret-refresh"));
        QJSEngine::setObjectOwnership(&api, QJSEngine::CppOwnership);
        engine.globalObject().setProperty(QStringLiteral("Api"), engine.newQObject(&api));
        engine.evaluate(QStringLiteral("var got = null; Api.probeServer('http://127.0.0.1:%1', function(r) { got = r })").arg(m_port));
        QTRY_VERIFY_WITH_TIMEOUT(engine.evaluate(QStringLiteral("got !== null")).toBool(), 5000);
        QVERIFY(engine.evaluate(QStringLiteral("got.ok")).toBool());
        QCOMPARE(engine.evaluate(QStringLiteral("got.data.url")).toString(), QStringLiteral("http://127.0.0.1:%1").arg(m_port));
        QCOMPARE(engine.evaluate(QStringLiteral("got.data.version")).toString(), QStringLiteral("3.0.0"));
        {
            std::lock_guard<std::mutex> l(m_mutex);
            QCOMPARE(m_metaAuth, QString());  // never the user's token
        }
        // Another service on the same port range answers, but not as Nhill.
        engine.evaluate(QStringLiteral("got = null; Api.probeServer('http://127.0.0.1:%1', function(r) { got = r })").arg(m_otherPort));
        QTRY_VERIFY_WITH_TIMEOUT(engine.evaluate(QStringLiteral("got !== null")).toBool(), 5000);
        QCOMPARE(engine.evaluate(QStringLiteral("got.error.code")).toString(), QStringLiteral("not_nhill_server"));
    }

    // A new rights version (plan or role changed) is announced once per change.
    void rightsVersionChangesAreAnnounced() {
        ApiClient api;
        api.setBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(m_port));
        QSignalSpy spy(&api, &ApiClient::rightsChanged);
        auto call = [&](const char* path) {
            bool done = false;
            ApiClient::Request r;
            r.path = QString::fromLatin1(path);
            api.request(r, [&](const ApiResult&) { done = true; });
            QTRY_VERIFY_WITH_TIMEOUT(done, 5000);
        };
        m_rights = "v1";
        call("/rights");   // first version seen: nothing to announce
        call("/rights");
        QCOMPARE(spy.count(), 0);
        m_rights = "v2";
        call("/rights");
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("v2"));
        call("/refused");  // 403 feature_not_included: rights are reloaded as well
        QCOMPARE(spy.count(), 2);
    }

    void initTestCase() {
        m_server.Get("/api/v1/meta", [this](const httplib::Request& req, httplib::Response& res) {
            std::lock_guard<std::mutex> l(m_mutex);
            m_metaAuth = QString::fromStdString(req.get_header_value("Authorization"));
            res.set_content("{\"product\":\"Nhill\",\"version\":\"3.0.0\",\"api\":\"v1\",\"min_client_version\":\"3.0.0\"}",
                            "application/json");
        });
        m_server.Get("/rights", [this](const httplib::Request&, httplib::Response& res) {
            res.set_header("X-Nhill-Rights", m_rights.load() == nullptr ? "" : m_rights.load());
            res.set_content("{}", "application/json");
        });
        m_server.Get("/refused", [](const httplib::Request&, httplib::Response& res) {
            res.status = 403;
            res.set_content("{\"error\":{\"code\":\"feature_not_included\"}}", "application/json");
        });
        m_other.Get("/api/v1/meta", [](const httplib::Request&, httplib::Response& res) {
            res.set_content("{\"name\":\"something else\"}", "application/json");
        });
        m_otherPort = m_other.bind_to_any_port("127.0.0.1");
        m_otherThread = std::thread([this] { m_other.listen_after_bind(); });
        m_server.Post("/echo", [this](const httplib::Request& req, httplib::Response& res) {
            std::lock_guard<std::mutex> l(m_mutex);
            m_lastBody = QByteArray::fromStdString(req.body);
            m_lastKey = QString::fromStdString(req.get_header_value("Idempotency-Key"));
            res.set_content("{\"ok\":true}", "application/json");
        });
        auto slow = [this](const httplib::Request&, httplib::Response& res) {
            ++m_slowHits;
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
            res.set_content("{}", "application/json");
        };
        m_server.Post("/slow", slow);
        m_server.Get("/slow", slow);
        m_server.Post("/api/v1/auth/refresh", [this](const httplib::Request&, httplib::Response& res) {
            ++m_refreshHits;
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            res.set_content("{\"access_token\":\"new-access\",\"refresh_token\":\"new-refresh\"}", "application/json");
        });
        m_server.Post("/protected", [this](const httplib::Request& req, httplib::Response& res) {
            if (req.get_header_value("Authorization") != "Bearer new-access") {
                res.status = 401;
                res.set_content("{\"error\":{\"code\":\"invalid_token\"}}", "application/json");
                return;
            }
            std::lock_guard<std::mutex> l(m_mutex);
            m_replayedKeys << QString::fromStdString(req.get_header_value("Idempotency-Key"));
            res.set_content("{\"ok\":true}", "application/json");
        });
        m_port = m_server.bind_to_any_port("127.0.0.1");
        QVERIFY(m_port > 0);
        m_thread = std::thread([this] { m_server.listen_after_bind(); });
    }
    void cleanupTestCase() {
        m_server.stop();
        m_other.stop();
        if (m_thread.joinable()) m_thread.join();
        if (m_otherThread.joinable()) m_otherThread.join();
    }

    // Regression: a JS object passed from QML arrives as QVariant<QJSValue>.
    void sendsJsObjectBodiesFromQml() {
        QQmlEngine engine;
        ApiClient api;
        api.setBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(m_port));
        QJSEngine::setObjectOwnership(&api, QJSEngine::CppOwnership);
        engine.globalObject().setProperty(QStringLiteral("Api"), engine.newQObject(&api));
        engine.evaluate(QStringLiteral(
            "var last = null; Api.send('POST', '/echo', { email: 'a@b.cd', lines: [{ quantity: '2.500' }], n: 3 },"
            " function(r) { last = r }, 'key-123')"));
        QTRY_VERIFY_WITH_TIMEOUT(engine.evaluate(QStringLiteral("last !== null")).toBool(), 5000);
        QVERIFY(engine.evaluate(QStringLiteral("last.ok")).toBool());
        std::lock_guard<std::mutex> l(m_mutex);
        QJsonObject body = QJsonDocument::fromJson(m_lastBody).object();
        QCOMPARE(body.value(QStringLiteral("email")).toString(), QStringLiteral("a@b.cd"));
        QCOMPARE(body.value(QStringLiteral("lines")).toArray().at(0).toObject().value(QStringLiteral("quantity")).toString(),
                 QStringLiteral("2.500"));
        QCOMPARE(body.value(QStringLiteral("n")).toInt(), 3);
        QCOMPARE(m_lastKey, QStringLiteral("key-123"));
    }

    // A write whose answer is lost is reported "uncertain" and never replayed automatically.
    void writesAreNeverRetriedAutomatically() {
        ApiClient api;
        api.setBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(m_port));
        api.setTimeoutMs(250);
        m_slowHits = 0;
        ApiResult got;
        bool done = false;
        ApiClient::Request r;
        r.method = "POST";
        r.path = QStringLiteral("/slow");
        r.body = QJsonObject{{QStringLiteral("x"), 1}};
        r.idempotencyKey = QStringLiteral("k-1");
        api.request(r, [&](const ApiResult& res) { got = res; done = true; });
        QTRY_VERIFY_WITH_TIMEOUT(done, 5000);
        QVERIFY(got.network);
        QVERIFY(got.uncertain);
        QCOMPARE(got.idempotencyKey, QStringLiteral("k-1"));
        QTest::qWait(1500);
        QCOMPARE(m_slowHits.load(), 1);
    }

    void readsAreRetried() {
        ApiClient api;
        api.setBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(m_port));
        api.setTimeoutMs(250);
        m_slowHits = 0;
        bool done = false;
        ApiResult got;
        ApiClient::Request r;
        r.path = QStringLiteral("/slow");
        api.request(r, [&](const ApiResult& res) { got = res; done = true; });
        QTRY_VERIFY_WITH_TIMEOUT(done, 10000);
        QVERIFY(!got.uncertain);
        QCOMPARE(m_slowHits.load(), 3);  // first attempt + 2 retries
    }

    // Two requests hit an expired access token: one refresh only, then both are
    // replayed with their original idempotency keys.
    void singleFlightRefreshKeepsKeys() {
        ApiClient api;
        api.setBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(m_port));
        api.setTokens(QStringLiteral("expired"), QStringLiteral("refresh-1"));
        QSignalSpy refreshed(&api, &ApiClient::tokensRefreshed);
        m_refreshHits = 0;
        m_replayedKeys.clear();
        int done = 0;
        for (const char* key : {"A", "B"}) {
            ApiClient::Request r;
            r.method = "POST";
            r.path = QStringLiteral("/protected");
            r.body = QJsonObject{};
            r.idempotencyKey = QString::fromLatin1(key);
            api.request(r, [&](const ApiResult& res) { QVERIFY(res.ok); ++done; });
        }
        QTRY_COMPARE_WITH_TIMEOUT(done, 2, 5000);
        QCOMPARE(m_refreshHits.load(), 1);
        QCOMPARE(refreshed.count(), 1);
        QCOMPARE(api.refreshToken(), QStringLiteral("new-refresh"));
        std::lock_guard<std::mutex> l(m_mutex);
        QStringList keys = m_replayedKeys;
        keys.sort();
        QCOMPARE(keys, (QStringList{QStringLiteral("A"), QStringLiteral("B")}));
    }

private:
    httplib::Server m_server, m_other;
    std::thread m_thread, m_otherThread;
    int m_port = 0, m_otherPort = 0;
    QString m_metaAuth;
    std::atomic<const char*> m_rights{nullptr};
    std::mutex m_mutex;
    QByteArray m_lastBody;
    QString m_lastKey;
    QStringList m_replayedKeys;
    std::atomic<int> m_slowHits{0}, m_refreshHits{0};
#endif
};

QTEST_MAIN(ClientTest)
#include "test_client.moc"
