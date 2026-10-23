// plate — design a microplate layout and export it for a plate reader or liquid handler.

#include <QCommandLineParser>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>
#include <QtQml>

#include "backend.h"
#include "cli.h"
#include "plateview.h"
#include "portalfilepicker.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[]) {
    // Commands and help draw no window, so they must not need a display, even
    // where the desktop exports QT_QPA_PLATFORM=wayland. Bare plate prints
    // help, as a command line tool should; the launcher runs plate open.
    const bool command = argc == 1 || isCliCommand(argv[1]);
    bool windowless = command;
    for (int i = 1; i < argc; ++i) {
        const QByteArray argument(argv[i]);
        windowless = windowless || argument == "--help" || argument == "-h" || argument == "--version" || argument == "-v";
    }
    if (windowless) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        // Nothing to show, so no desktop integration to load or complain about.
        qputenv("QT_QPA_PLATFORMTHEME", "generic");
        qputenv("NO_AT_BRIDGE", "1");
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName("plate");
    app.setApplicationVersion("0.2.0");

    // Matches the window to plate.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("plate");
    app.setWindowIcon(QIcon::fromTheme("plate"));

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    if (command)
        return runCli(app.arguments());

    QCommandLineParser args;
    args.setApplicationDescription("Design a microplate layout and export it for a plate reader or liquid handler.\n\n"
                                   + cliSummary());
    args.addHelpOption();
    args.addVersionOption();
    args.addPositionalArgument("file", "Plate layout (.plate) to open", "[file]");
    QStringList arguments = app.arguments();
    if (arguments.value(1) == "open")
        arguments.removeAt(1);
    args.process(arguments);

    // The dense variant suits a desktop window full of small controls.
    qputenv("QT_QUICK_CONTROLS_MATERIAL_VARIANT", "Dense");
    QQuickStyle::setStyle("Material");
    qmlRegisterType<PlateView>("Plate", 1, 0, "PlateView");
    qmlRegisterType<WellSwatch>("Plate", 1, 0, "WellSwatch");

    Theme theme;
    PortalFilePicker picker;
    Backend backend(&picker);
    backend.setAccent(QColor(theme.accent()));
    QObject::connect(&theme, &Theme::changed, &backend, [&] { backend.setAccent(QColor(theme.accent())); });
    const QString file = args.positionalArguments().value(0);
    if (!file.isEmpty())
        backend.openPath(QFileInfo(file).absoluteFilePath());

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
