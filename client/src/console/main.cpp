// Nhill administration console (creators only). Separate executable,
// separate authentication (password + TOTP), platform data only.
#include <QFile>
#include <QFont>
#include <QJsonDocument>
#include <QJsonObject>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QtQml/qqmlextensionplugin.h>

#include "appcontext.h"
#include "nhill/version.hpp"

Q_IMPORT_QML_PLUGIN(Nhill_UiPlugin)
Q_IMPORT_QML_PLUGIN(Nhill_ConsolePlugin)

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("Nhill"));
    QGuiApplication::setApplicationName(QStringLiteral("Nhill Console"));
    QGuiApplication::setApplicationVersion(QString::fromLatin1(nhill::kProductVersion));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/brand/nhill-icon-console.svg")));
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    nhc::registerFonts();
    QFont f(QStringLiteral("IBM Plex Sans"));
    f.setPixelSize(14);
    QGuiApplication::setFont(f);

    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/"));
    nhc::AppContext ctx(&engine);
    ctx.registerTypes();
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    // Screenshot tour for documentation / visual QA (development): needs both a
    // target folder and an already-open console session token.
    QVariantMap tour;
    if (!qEnvironmentVariable("NHILL_TOUR_DIR").isEmpty()) {
        tour["dir"] = qEnvironmentVariable("NHILL_TOUR_DIR");
        tour["token"] = qEnvironmentVariable("NHILL_ADMIN_TOUR_TOKEN");
        tour["establishment"] = qEnvironmentVariable("NHILL_TOUR_ESTABLISHMENT");
        tour["ticket"] = qEnvironmentVariable("NHILL_TOUR_TICKET");
    }
    engine.rootContext()->setContextProperty(QStringLiteral("adminTour"), tour);
    // Installation-wizard tour (development): JSON file with the server, the setup token,
    // the database fields and the capture folder. Drives the real wizard end to end.
    QVariantMap setupTour;
    if (!qEnvironmentVariable("NHILL_SETUP_TOUR").isEmpty()) {
        QFile f(qEnvironmentVariable("NHILL_SETUP_TOUR"));
        if (f.open(QIODevice::ReadOnly)) setupTour = QJsonDocument::fromJson(f.readAll()).object().toVariantMap();
    }
    engine.rootContext()->setContextProperty(QStringLiteral("setupTour"), setupTour);
    engine.load(QUrl(QStringLiteral("qrc:/Nhill/Console/ConsoleMain.qml")));
    return app.exec();
}
