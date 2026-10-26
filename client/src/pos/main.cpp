// Nhill POS — point of sale attached to a Nhill Business establishment (Windows, Linux).
#include <QFont>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QtQml/qqmlextensionplugin.h>

#include "appcontext.h"
#include "nhill/version.hpp"

Q_IMPORT_QML_PLUGIN(Nhill_UiPlugin)
Q_IMPORT_QML_PLUGIN(Nhill_PosPlugin)

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("Nhill"));
    QGuiApplication::setApplicationName(QStringLiteral("Nhill POS"));
    QGuiApplication::setApplicationVersion(QString::fromLatin1(nhill::kProductVersion));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/brand/nhill-icon-pos.svg")));
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    nhc::registerFonts();
    QFont f(QStringLiteral("IBM Plex Sans"));
    f.setPixelSize(14);
    QGuiApplication::setFont(f);

    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/"));
    nhc::AppContext ctx(&engine);
    ctx.registerTypes();

    // Optional automated UI tour (screenshots for documentation / visual QA).
    QVariantMap tour;
    tour["dir"] = qEnvironmentVariable("NHILL_TOUR_DIR");
    tour["email"] = qEnvironmentVariable("NHILL_TOUR_EMAIL");
    tour["password"] = qEnvironmentVariable("NHILL_TOUR_PASSWORD");
    tour["tag"] = qEnvironmentVariable("NHILL_TOUR_TAG");
    // "390x844" -> phone-sized window for the tour screenshots.
    const QStringList size = qEnvironmentVariable("NHILL_TOUR_SIZE").split(QLatin1Char('x'));
    if (size.size() == 2) {
        tour["width"] = size[0].toInt();
        tour["height"] = size[1].toInt();
    }
    engine.rootContext()->setContextProperty(QStringLiteral("tourConfig"), tour);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/Nhill/Pos/PosMain.qml")));
    return app.exec();
}
