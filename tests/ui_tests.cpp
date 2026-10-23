// Drives the real window offscreen with the mouse and keyboard, as a person
// would. Set PLATE_UI_SHOTS to a directory to keep a screenshot of each test.
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <memory>

#include "backend.h"
#include "filepicker.h"
#include "plateview.h"
#include "theme.h"
#include "translator.h"

namespace {
// Answers like the portal once a file is chosen; with no path, like a cancelled chooser.
class FakePicker : public FilePicker {
public:
    QString path;
    QString format = "csv";
    void openPlate() override { answer([this](const QUrl &url) { emit openSelected(url); }); }
    void savePlate(const QUrl &) override { answer([this](const QUrl &url) { emit saveSelected(url); }); }
    void exportPlate(const QUrl &) override { answer([this](const QUrl &url) { emit exportSelected(url, format); }); }
    void openNames() override { answer([this](const QUrl &url) { emit namesSelected(url); }); }

private:
    template <typename Emit> void answer(Emit emitSignal) {
        if (!path.isEmpty())
            emitSignal(QUrl::fromLocalFile(path));
    }
};

void write(const QString &path, const QByteArray &bytes) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(bytes);
}

// One window, its backend, and a private theme, language, and template directory.
struct Session {
    QTemporaryDir dir;
    FakePicker picker;
    std::unique_ptr<Translator> translator;
    std::unique_ptr<Theme> theme;
    std::unique_ptr<Backend> backend;
    std::unique_ptr<QQmlApplicationEngine> engine;
    QQuickWindow *window = nullptr;
    QQuickItem *view = nullptr;

    explicit Session(const QString &language = "en", QSize size = QSize(1280, 760)) {
        write(dir.filePath("language"), language.toUtf8());
        write(dir.filePath("state/theme/colors.toml"), "accent = \"#FFD60A\"\n");
        translator = std::make_unique<Translator>(dir.filePath("language"));
        qApp->installTranslator(translator.get());
        theme = std::make_unique<Theme>(dir.filePath("state"), nullptr, "/bin/false");
        backend = std::make_unique<Backend>(&picker);
        backend->setTemplateDirectory(dir.filePath("templates"));
        backend->setAccent(QColor(theme->accent()));
        QObject::connect(theme.get(), &Theme::changed, backend.get(), [this] { backend->setAccent(QColor(theme->accent())); });
        engine = std::make_unique<QQmlApplicationEngine>();
        QObject::connect(translator.get(), &Translator::changed, engine.get(), &QQmlEngine::retranslate);
        engine->rootContext()->setContextProperty("translator", translator.get());
        engine->rootContext()->setContextProperty("theme", theme.get());
        engine->rootContext()->setContextProperty("backend", backend.get());
        engine->load(QUrl("qrc:/Main.qml"));
        window = qobject_cast<QQuickWindow *>(engine->rootObjects().value(0));
        if (!window)
            return;
        window->resize(size);
        window->show();
        settle();
        view = window->findChild<PlateView *>();
    }
    ~Session() {
        if (window)
            window->close();
        engine.reset();
        qApp->removeTranslator(translator.get());
    }

    void settle(int rounds = 10) {
        for (int i = 0; i < rounds; ++i) {
            QCoreApplication::processEvents();
            QTest::qWait(15);
        }
    }
    void key(int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier, const QString &text = {}) {
        QKeyEvent press(QEvent::KeyPress, key, modifiers, text), release(QEvent::KeyRelease, key, modifiers, text);
        QCoreApplication::sendEvent(window, &press);
        QCoreApplication::sendEvent(window, &release);
        settle();
    }
    // Single-key shortcuts, typed as their character.
    void press(QChar c) { key(c.toUpper().unicode(), Qt::NoModifier, QString(c)); }
    void type(const QString &text) {
        for (QChar c : text) {
            int k = c.toUpper().unicode();
            if (c == ' ')
                k = Qt::Key_Space;
            key(k, c.isUpper() ? Qt::ShiftModifier : Qt::NoModifier, QString(c));
        }
    }
    // Dialogs fade in and out; a person waits for them, and so does the test.
    void waitForDialog(bool open) {
        QTRY_VERIFY_WITH_TIMEOUT(window->property("dialogOpen").toBool() == open, 2000);
        QTest::qWait(300);
        settle();
    }
    void enter() {
        key(Qt::Key_Return, Qt::NoModifier, "\r");
        QTest::qWait(250);
        settle();
    }
    void escape() {
        key(Qt::Key_Escape);
        QTest::qWait(250);
        settle();
    }
    QPoint well(const QString &name) {
        const int row = name[0].unicode() - 'A', column = name.mid(1).toInt() - 1;
        QPointF center;
        QMetaObject::invokeMethod(view, "wellCenter", Q_RETURN_ARG(QPointF, center), Q_ARG(int, row), Q_ARG(int, column));
        return view->mapToScene(center).toPoint();
    }
    void click(const QString &name, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        QTest::mouseClick(window, Qt::LeftButton, modifiers, well(name));
        settle();
    }
    void drag(const QString &from, const QString &to, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        QTest::mousePress(window, Qt::LeftButton, modifiers, well(from));
        QTest::mouseMove(window, well(to));
        QTest::mouseRelease(window, Qt::LeftButton, modifiers, well(to));
        settle();
    }
    void hover(const QString &name) {
        QTest::mouseMove(window, well(name));
        settle();
    }
    void clickItem(QQuickItem *item) {
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
        QTest::qWait(250);
        settle();
    }
    QString text(const char *objectName) {
        auto *item = window->findChild<QQuickItem *>(objectName);
        return item ? item->property("text").toString() : QString();
    }
    // Every item on screen, dialogs included: the overlay is a child of the root item too.
    QList<QQuickItem *> items() {
        QList<QQuickItem *> all, pending{window->contentItem()};
        while (!pending.isEmpty()) {
            QQuickItem *item = pending.takeFirst();
            if (!item->isVisible())
                continue;
            all.append(item);
            pending.append(item->childItems());
        }
        return all;
    }
    // The visible button whose text starts with prefix.
    QQuickItem *control(const QString &prefix) {
        for (auto *item : items())
            if (item->property("checkable").isValid() && item->property("text").toString().startsWith(prefix))
                return item;
        return nullptr;
    }
    QQuickItem *field(const QString &placeholder) {
        for (auto *item : items())
            if (item->property("placeholderText").toString().startsWith(placeholder))
                return item;
        return nullptr;
    }
    QObject *dialog(const char *signal) {
        for (auto *object : window->findChildren<QObject *>())
            if (object->metaObject()->indexOfSignal(signal) >= 0)
                return object;
        return nullptr;
    }
    bool typing() {
        auto *focus = window->activeFocusItem();
        return focus && focus->property("cursorPosition").isValid();
    }
    void shot(const QString &name) {
        const QString directory = qEnvironmentVariable("PLATE_UI_SHOTS");
        if (directory.isEmpty())
            return;
        QTest::qWait(400);
        settle();
        QDir().mkpath(directory);
        window->grabWindow().save(directory + "/" + name + ".png");
    }
};
} // namespace

class UiTests : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qputenv("QT_QUICK_CONTROLS_MATERIAL_VARIANT", "Dense");
        QQuickStyle::setStyle("Material");
        qmlRegisterType<PlateView>("Plate", 1, 0, "PlateView");
        qmlRegisterType<WellSwatch>("Plate", 1, 0, "WellSwatch");
    }

    // An ELISA layout from scratch, with only the mouse and keyboard.
    void buildsAnElisaLayout()
    {
        Session s;
        QVERIFY(s.view);
        Backend &b = *s.backend;

        s.clickItem(s.field("Unit"));
        s.type("ng/mL");
        s.enter();
        QCOMPARE(b.unit(), QString("ng/mL"));

        // An 8-point standard curve down column 1, halving from 1000, in duplicate.
        s.press('2');
        s.drag("A1", "H1");
        s.press('d');
        s.type("1000");
        s.key(Qt::Key_Tab);
        s.type("2"); // Tabbing in selects the old factor, so this replaces it.
        s.enter();
        s.press('l');
        s.type("Std");
        s.enter();
        s.press('r');
        QCOMPARE(b.selectionName(), QString("A2:H2"));
        QCOMPARE(*b.plate().well(7, 1).concentration, 7.8125);

        s.press('3'); s.drag("A3", "B3"); s.press('l'); s.type("Pos ctrl"); s.enter();
        s.press('4'); s.drag("C3", "D3"); s.press('l'); s.type("Neg ctrl"); s.enter();
        s.press('5'); s.drag("E3", "H3"); s.press('l'); s.type("Blank"); s.enter();
        s.press('1'); s.drag("A4", "H11");
        for (int pair = 0; pair < 4; ++pair) {
            s.drag(QString("A%1").arg(4 + pair * 2), QString("A%1").arg(5 + pair * 2), Qt::ControlModifier);
            s.press('l');
            s.type(QString("Donor %1").arg(pair + 1, 2, 10, QChar('0')));
            s.enter();
        }
        QCOMPARE(b.plate().count(WellType::Sample), 64);
        QCOMPARE(b.plate().count(WellType::Empty), 8);

        // A slip, undone and redone with each redo key.
        s.press('6');
        s.click("B4");
        QCOMPARE(b.plate().well(1, 3).type, WellType::Empty);
        s.key(Qt::Key_Z, Qt::ControlModifier);
        QCOMPARE(b.plate().well(1, 3).type, WellType::Sample);
        s.key(Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(b.plate().well(1, 3).type, WellType::Empty);
        s.key(Qt::Key_Z, Qt::ControlModifier);
        s.key(Qt::Key_Y, Qt::ControlModifier);
        QCOMPARE(b.plate().well(1, 3).type, WellType::Empty);
        s.key(Qt::Key_Z, Qt::ControlModifier);
        QCOMPARE(b.plate().well(0, 9).label, QString("Donor 04"));

        s.press('1');
        s.drag("A4", "A5", Qt::ControlModifier);
        s.hover("C1");
        QCOMPARE(s.text("status"), QString("C1  ·  Standard  ·  Std  ·  250 ng/mL"));
        s.shot("elisa");

        // Save, export each format, reopen, and quit.
        s.picker.path = s.dir.filePath("elisa.plate");
        s.key(Qt::Key_S, Qt::ControlModifier);
        QVERIFY(!b.dirty());
        for (const QString format : {"csv", "svg", "pdf"}) {
            s.picker.path = s.dir.filePath("elisa." + format);
            s.picker.format = format;
            s.key(Qt::Key_E, Qt::ControlModifier);
            QVERIFY2(QFileInfo(s.picker.path).size() > 0, qPrintable(b.message()));
        }
        QFile csv(s.dir.filePath("elisa.csv"));
        QVERIFY(csv.open(QIODevice::ReadOnly));
        const QByteArray lines = csv.readAll();
        QCOMPARE(lines.count("\r\n"), 97);
        QVERIFY(lines.contains("H2,Standard,Std,7.8125\r\n"));
        QVERIFY(lines.contains("A10,Sample,Donor 04,\r\n"));

        const Plate saved = b.plate();
        b.newPlate(384);
        s.picker.path = s.dir.filePath("elisa.plate");
        s.key(Qt::Key_O, Qt::ControlModifier);
        QCOMPARE(b.plate(), saved);
        QSignalSpy quit(qApp, &QCoreApplication::aboutToQuit);
        s.press('q');
        QVERIFY(s.window->property("quitting").toBool());
    }

    // The same, on a 384-well plate picked from the New plate dialog.
    void handlesA384WellPlate()
    {
        Session s;
        Backend &b = *s.backend;
        s.key(Qt::Key_N, Qt::ControlModifier);
        s.waitForDialog(true);
        s.key(Qt::Key_Down);
        s.enter();
        QCOMPARE(b.rows(), 16);
        QCOMPARE(b.columns(), 24);
        s.press('2');
        s.drag("A1", "A24");
        s.press('d');
        s.type("100");
        s.key(Qt::Key_Tab);
        s.type("3");
        s.enter();
        QCOMPARE(b.plate().count(WellType::Standard), 24);
        QCOMPARE(*b.plate().well(0, 23).concentration, 100 / std::pow(3.0, 23));
        s.key(Qt::Key_R, Qt::ShiftModifier, "R");
        QCOMPARE(b.selectionName(), QString("B1:B24"));
        s.press('1');
        s.drag("C1", "P24");
        // Shift+drag down a column fills the whole column.
        s.press('5');
        s.drag("C24", "F24", Qt::ShiftModifier);
        QCOMPARE(b.plate().count(WellType::Blank), 16);
        s.hover("P24");
        QCOMPARE(s.text("status"), QString("P24  ·  Blank"));
        s.shot("384");
        QCOMPARE(b.plate().toCsv().count("\r\n"), 385);
    }

    void pastesCopiesClearsAndImportsNames()
    {
        Session s;
        Backend &b = *s.backend;
        // Names copied from a spreadsheet: a header and two columns.
        QGuiApplication::clipboard()->setText("Sample\tSample\nP-01\tP-02\nP-03\tP-04\n");
        s.click("B2", Qt::ControlModifier);
        s.key(Qt::Key_V, Qt::ControlModifier);
        QCOMPARE(b.selectionName(), QString("B2:C3"));
        QCOMPARE(b.plate().well(2, 2).label, QString("P-04"));
        QCOMPARE(b.plate().count(WellType::Sample), 4);
        QVERIFY(b.message().startsWith("Pasted 4 names into B2:C3"));

        s.key(Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(QGuiApplication::clipboard()->text(), QString("P-01\tP-02\nP-03\tP-04"));

        s.key(Qt::Key_Delete);
        QCOMPARE(b.plate(), Plate());
        s.key(Qt::Key_Z, Qt::ControlModifier);
        QCOMPARE(b.plate().count(WellType::Sample), 4);
        s.key(Qt::Key_Backspace);
        QCOMPARE(b.plate(), Plate());

        // A semicolon CSV, as a Danish spreadsheet saves it, through Ctrl+I.
        s.picker.path = s.dir.filePath("names.csv");
        write(s.picker.path, "Name;Volume\nDonor 01;1,5\nDonor 02;2,5\n");
        s.click("A12", Qt::ControlModifier);
        s.key(Qt::Key_I, Qt::ControlModifier);
        QCOMPARE(b.plate().well(0, 11).label, QString("Donor 01"));
        QCOMPARE(b.plate().well(1, 11).label, QString("Donor 02"));
        // The second column runs off the plate's right edge.
        QVERIFY(b.messageIsError());
        QVERIFY(b.message().contains("2 didn't fit"));

        // Through the sidebar buttons too.
        s.clickItem(s.control("Clear"));
        QCOMPARE(b.plate().count(WellType::Sample), 0);
        s.clickItem(s.control("Import names"));
        QCOMPARE(b.plate().count(WellType::Sample), 2);
        QVERIFY(!s.typing());
    }

    void savesAndUsesTemplates()
    {
        Session s;
        Backend &b = *s.backend;
        s.press('5');
        s.drag("A12", "H12");
        s.key(Qt::Key_T, Qt::ControlModifier);
        QVERIFY(s.typing());
        s.type("Blank column");
        s.shot("template-save");
        s.enter();
        QVERIFY(QFile::exists(s.dir.filePath("templates/Blank column.plate")));

        b.newPlate(96);
        s.key(Qt::Key_N, Qt::ControlModifier);
        s.waitForDialog(true);
        s.shot("template-new");
        QQuickItem *choice = s.control("Blank column");
        QVERIFY(choice);
        s.clickItem(choice);
        QCOMPARE(b.plate().count(WellType::Blank), 8);
        QVERIFY(!b.dirty());
        QCOMPARE(b.fileName(), QString("Untitled plate"));

        // The built-in ELISA layout, picked with the keyboard: 96, 384, then ELISA.
        s.key(Qt::Key_N, Qt::ControlModifier);
        s.waitForDialog(true);
        s.key(Qt::Key_Down);
        s.key(Qt::Key_Down);
        s.enter();
        QCOMPARE(b.plate().count(WellType::Standard), 16);

        // Deleting takes two clicks.
        s.key(Qt::Key_N, Qt::ControlModifier);
        s.waitForDialog(true);
        QQuickItem *remove = s.control("×");
        QVERIFY(remove);
        s.clickItem(remove);
        QVERIFY(QFile::exists(s.dir.filePath("templates/Blank column.plate")));
        s.clickItem(s.control("Delete"));
        QVERIFY(!QFile::exists(s.dir.filePath("templates/Blank column.plate")));
        s.escape();
        s.waitForDialog(false);
    }

    // Each of these lost or changed work before the review fixes.
    void leavesFieldsAloneUntilTypedIn()
    {
        Session s;
        Backend &b = *s.backend;
        b.setCurrentType(int(WellType::Standard));
        b.select(0, 0, 0, 5);
        b.dilute(64, 2, 0);
        b.setCurrentType(int(WellType::Sample));
        b.assign(1, 0, 1, 0);
        b.setSelectionLabel("S1");
        b.assign(1, 1, 1, 1);
        b.setSelectionLabel("S2");
        const Plate before = b.plate();

        b.select(0, 0, 0, 5);
        s.press('c'); s.escape();
        s.press('c'); s.enter();
        b.select(1, 0, 1, 1);
        s.press('l'); s.escape();
        s.press('l'); s.enter();
        QCOMPARE(b.plate(), before);

        b.select(1, 0, 1, 0);
        s.press('l'); s.type("oops"); s.escape();
        QCOMPARE(b.plate().well(1, 0).label, QString("S1"));
        QVERIFY(!s.typing());
        s.press('c'); s.type("5"); s.escape();
        QVERIFY(!b.plate().well(1, 0).concentration);
    }

    void doesNotQuitOnALaterSave()
    {
        Session s;
        s.backend->assign(0, 0, 0, 0);
        s.press('q');
        QObject *unsaved = s.dialog("saveChosen()");
        QVERIFY(unsaved && unsaved->property("visible").toBool());
        QMetaObject::invokeMethod(unsaved, "saveChosen"); // The chooser is then cancelled.
        QMetaObject::invokeMethod(unsaved, "close");
        QTest::qWait(300);
        s.picker.path = s.dir.filePath("later.plate");
        s.key(Qt::Key_S, Qt::ControlModifier);
        QVERIFY(!s.backend->dirty());
        QVERIFY(!s.window->property("quitting").toBool());
    }

    void readsNumbersInTheLanguage_data()
    {
        QTest::addColumn<QString>("language");
        QTest::addColumn<QString>("typed");
        QTest::newRow("en point") << "en" << "1.5";
        QTest::newRow("en point zeros") << "en" << "1.500";
        QTest::newRow("da comma") << "da" << "1,5";
        QTest::newRow("da point") << "da" << "1.500";
    }
    void readsNumbersInTheLanguage()
    {
        QFETCH(QString, language);
        QFETCH(QString, typed);
        Session s(language);
        s.press('c');
        for (QChar c : typed)
            s.key(c == '.' ? Qt::Key_Period : c == ',' ? Qt::Key_Comma : c.unicode(), Qt::NoModifier, QString(c));
        s.enter();
        QCOMPARE(*s.backend->plate().well(0, 0).concentration, 1.5);
    }

    void reportsABrokenFile()
    {
        Session s;
        s.picker.path = s.dir.filePath("broken.plate");
        write(s.picker.path, "{nope");
        s.key(Qt::Key_O, Qt::ControlModifier);
        QVERIFY(s.backend->messageIsError());
        QVERIFY(s.text("status").startsWith("Could not open broken.plate"));
        QCOMPARE(s.backend->fileName(), QString("Untitled plate"));

        // A plate with a bad well still opens, and says what was wrong.
        write(s.picker.path, R"({"format":"plate","version":1,"rows":8,"columns":12,"wells":[
            {"well":"A1","type":"sample","label":"ok"},{"well":"Z9","type":"sample"}]})");
        s.key(Qt::Key_O, Qt::ControlModifier);
        QCOMPARE(s.backend->plate().well(0, 0).label, QString("ok"));
        QVERIFY(s.backend->messageIsError());
        QVERIFY(s.text("status").contains("1 problem"));
        s.shot("broken-file");
    }

    void keepsLongLabelsInsideTheirWells()
    {
        Session s;
        const QString name = "Patient 0042 serum, day 14 follow-up, diluted before freezing (aliquot B)";
        s.backend->select(0, 0, 1, 11);
        s.backend->setCurrentType(int(WellType::Sample));
        s.backend->assignSelection();
        s.backend->setSelectionLabel(name);
        s.hover("A1");
        QVERIFY(s.text("status").contains(name));
        s.shot("long-labels");
        // The CSV keeps the whole label, quoted for its comma.
        QVERIFY(s.backend->plate().toCsv().contains("A1,Sample,\"" + name.toUtf8() + "\","));
    }

    void fitsTheSmallestWindow()
    {
        Session s("da", QSize(820, 520));
        QCOMPARE(s.window->width(), 820);
        s.backend->select(0, 0, 0, 0);
        s.shot("smallest-da");
        // Every sidebar button can be reached: inside the window, or in its scrolling panel.
        for (const QString prefix : {"Fortynd", "Importér navne", "Ryd", "Ny", "Gem"}) {
            QQuickItem *button = s.control(prefix);
            QVERIFY2(button, qPrintable(prefix));
            QVERIFY2(button->width() >= button->implicitWidth() - 1, qPrintable(prefix + " is squeezed"));
        }
        s.press('?');
        s.waitForDialog(true);
        s.shot("smallest-help");
        // The last shortcut is reachable by scrolling with the keyboard.
        for (int i = 0; i < 20; ++i)
            s.key(Qt::Key_Down);
        s.shot("smallest-help-scrolled");
        bool quitShown = false;
        for (auto *item : s.items())
            if (item->property("text").toString() == "Afslut") {
                const QPointF bottom = item->mapToScene(QPointF(0, item->height()));
                quitShown = bottom.y() <= s.window->height();
            }
        QVERIFY(quitShown);
        s.press('?');
    }

    void followsThemeAndLanguageLive()
    {
        Session s;
        QSignalSpy themed(s.theme.get(), &Theme::changed);
        write(s.dir.filePath("state/theme/colors.toml"), "accent = \"#7aa2f7\"\n");
        QVERIFY(themed.wait(3000));
        s.settle();
        QCOMPARE(s.view->property("accent").value<QColor>(), QColor("#7aa2f7"));
        QCOMPARE(s.backend->accent(), QColor("#7aa2f7"));

        const QString english = s.text("hint");
        QSignalSpy translated(s.translator.get(), &Translator::changed);
        write(s.dir.filePath("language"), "da");
        QVERIFY(translated.wait(3000));
        s.settle();
        QVERIFY(s.text("hint") != english);
        QCOMPARE(s.backend->typeName(int(WellType::ControlPositive)), QString("Kontrol+"));
        s.shot("live-da");
    }

    // Every key the shortcut overlay lists does something.
    void answersEveryListedShortcut()
    {
        Session s;
        Backend &b = *s.backend;
        s.press('?');
        s.waitForDialog(true);
        s.shot("help");
        s.press('?');
        s.waitForDialog(false);
        for (int type = 1; type <= 6; ++type) {
            s.press(QChar('0' + type));
            QCOMPARE(b.currentType(), type - 1);
        }
        s.press('1');
        s.key(Qt::Key_Right);
        s.key(Qt::Key_Down);
        s.key(Qt::Key_Right, Qt::ShiftModifier);
        s.key(Qt::Key_Down, Qt::ShiftModifier);
        QCOMPARE(b.selectionName(), QString("B2:C3"));
        s.key(Qt::Key_Space, Qt::NoModifier, " ");
        QCOMPARE(b.plate().count(WellType::Sample), 4);
        s.key(Qt::Key_Left);
        s.key(Qt::Key_Up);
        QCOMPARE(b.selectionName(), QString("B2"));
        s.key(Qt::Key_Escape);
        QCOMPARE(b.selectionName(), QString("B2"));
        s.press('d');
        s.waitForDialog(true);
        s.escape();
        s.waitForDialog(false);
        s.key(Qt::Key_N, Qt::ControlModifier);
        s.waitForDialog(true); // Unsaved work: it asks first.
        QVERIFY(s.dialog("saveChosen()")->property("visible").toBool());
        s.escape();
        s.waitForDialog(false);
        s.key(Qt::Key_T, Qt::ControlModifier);
        s.waitForDialog(true);
        s.escape();
        s.waitForDialog(false);
        QVERIFY(!s.typing());
    }
};

QTEST_MAIN(UiTests)
#include "ui_tests.moc"
