#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "filepicker.h"
#include "plate.h"
#include "platepainter.h"
#include "templates.h"
#include "theme.h"
#include "translator.h"

namespace {
// Stands in for the desktop portal: answers every request with a fixed path.
class FakePicker : public FilePicker {
public:
    QString path;
    QString format = "csv";
    void openPlate() override { emit openSelected(QUrl::fromLocalFile(path)); }
    void savePlate(const QUrl &) override { emit saveSelected(QUrl::fromLocalFile(path)); }
    void exportPlate(const QUrl &) override { emit exportSelected(QUrl::fromLocalFile(path), format); }
    void openNames() override { emit namesSelected(QUrl::fromLocalFile(path)); }
};

QByteArray readAll(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
} // namespace

class PlateTests : public QObject {
    Q_OBJECT

private slots:
    void namesWellsLikeThePlastic()
    {
        QCOMPARE(Plate::wellName(0, 0), QString("A1"));
        QCOMPARE(Plate::wellName(7, 11), QString("H12"));
        QCOMPARE(Plate::wellName(15, 23), QString("P24"));
        QCOMPARE(Plate::rangeName(QRect(1, 2, 3, 2)), QString("C2:D4"));

        Plate small(8, 12);
        int row = -1, column = -1;
        QVERIFY(small.parseWellName("h12", &row, &column));
        QCOMPARE(row, 7);
        QCOMPARE(column, 11);
        QVERIFY(small.parseWellName("B03", &row, &column));
        QCOMPARE(column, 2);
        QVERIFY(!small.parseWellName("I1", &row, &column));
        QVERIFY(!small.parseWellName("A13", &row, &column));
        QVERIFY(Plate(16, 24).parseWellName("P24", &row, &column));
    }

    void dilutesAcrossARow()
    {
        Plate plate;
        plate.dilute(QRect(0, 1, 6, 1), 100, 2, Direction::Right, WellType::Standard);
        const double expected[] = {100, 50, 25, 12.5, 6.25, 3.125};
        for (int column = 0; column < 6; ++column) {
            QVERIFY(plate.well(1, column).concentration.has_value());
            QCOMPARE(*plate.well(1, column).concentration, expected[column]);
            QCOMPARE(plate.well(1, column).type, WellType::Standard);
        }
        QVERIFY(!plate.well(1, 6).concentration);
        QVERIFY(!plate.well(0, 0).concentration);
    }

    void dilutesEveryRowOfABlockAlike()
    {
        Plate plate;
        plate.well(0, 0).type = WellType::Sample;
        plate.dilute(QRect(0, 0, 4, 3), 1000, 10, Direction::Right, WellType::Standard);
        for (int row = 0; row < 3; ++row)
            QCOMPARE(*plate.well(row, 3).concentration, 1.0);
        // A well that already had a type keeps it.
        QCOMPARE(plate.well(0, 0).type, WellType::Sample);
        QCOMPARE(plate.well(1, 0).type, WellType::Standard);
    }

    void dilutesInEveryDirection()
    {
        Plate plate;
        plate.dilute(QRect(0, 0, 3, 1), 9, 3, Direction::Left, WellType::Standard);
        QCOMPARE(*plate.well(0, 2).concentration, 9.0);
        QCOMPARE(*plate.well(0, 0).concentration, 1.0);

        plate.dilute(QRect(5, 0, 1, 4), 8, 2, Direction::Down, WellType::Standard);
        QCOMPARE(*plate.well(0, 5).concentration, 8.0);
        QCOMPARE(*plate.well(3, 5).concentration, 1.0);

        plate.dilute(QRect(6, 0, 1, 4), 8, 2, Direction::Up, WellType::Standard);
        QCOMPARE(*plate.well(3, 6).concentration, 8.0);
        QCOMPARE(*plate.well(0, 6).concentration, 1.0);

        // A factor below one concentrates instead.
        plate.dilute(QRect(0, 7, 3, 1), 1, 0.5, Direction::Right, WellType::Standard);
        QCOMPARE(*plate.well(7, 2).concentration, 4.0);
    }

    void replicatesBesideTheSelection()
    {
        Plate plate;
        plate.well(0, 0) = {WellType::Sample, "Lysate 1", 5.0};
        plate.well(1, 0) = {WellType::ControlPositive, "Pos", std::nullopt};
        const QRect source(0, 0, 1, 2);

        QCOMPARE(plate.replicate(source, Direction::Right), QRect(1, 0, 1, 2));
        QCOMPARE(plate.well(0, 1), plate.well(0, 0));
        QCOMPARE(plate.well(1, 1), plate.well(1, 0));

        QCOMPARE(plate.replicate(QRect(0, 0, 2, 2), Direction::Down), QRect(0, 2, 2, 2));
        QCOMPARE(plate.well(3, 1).label, QString("Pos"));
        QCOMPARE(plate.well(2, 1).concentration, std::optional<double>(5.0));
        // Nothing else changes.
        QCOMPARE(plate.well(0, 2), Well());
        QCOMPARE(plate.well(4, 0), Well());
    }

    void refusesAReplicateWithNoRoom()
    {
        Plate plate;
        plate.well(0, 11).type = WellType::Sample;
        const Plate before = plate;
        QVERIFY(plate.replicaRect(QRect(11, 0, 1, 1), Direction::Right).isEmpty());
        QVERIFY(plate.replicate(QRect(11, 0, 1, 1), Direction::Right).isEmpty());
        QVERIFY(plate.replicate(QRect(0, 4, 12, 5), Direction::Down).isEmpty());
        QCOMPARE(plate, before);
        QCOMPARE(plate.replicaRect(QRect(0, 0, 6, 1), Direction::Right), QRect(6, 0, 6, 1));
    }

    void roundTripsThroughJson()
    {
        Plate plate(16, 24);
        plate.setUnit("µM");
        plate.well(0, 0) = {WellType::Standard, "Std 1", 100.0};
        plate.well(15, 23) = {WellType::Blank, QString(), std::nullopt};
        plate.well(3, 4) = {WellType::Sample, "Ünïcode, \"quoted\"", 0.0};
        plate.well(7, 8) = {WellType::ControlNegative, "Neg", std::nullopt};

        QVector<PlateProblem> problems;
        const auto read = Plate::fromJson(plate.toJsonBytes(), &problems);
        QVERIFY(read.has_value());
        QVERIFY(problems.isEmpty());
        QCOMPARE(*read, plate);
        // Zero is a concentration; unset is not.
        QCOMPARE(read->well(3, 4).concentration, std::optional<double>(0.0));
        QVERIFY(!read->well(15, 23).concentration);
        // Only wells that hold something are written.
        QCOMPARE(plate.toJson().value("wells").toArray().size(), 4);
    }

    void reportsEveryProblemInAFile()
    {
        QVector<PlateProblem> problems;
        QVERIFY(!Plate::fromJson("{nope", &problems));
        QCOMPARE(problems.size(), 1);

        problems.clear();
        QVERIFY(!Plate::fromJson(R"({"format":"plate","version":1,"rows":10,"columns":10,"wells":[]})", &problems));

        problems.clear();
        const auto plate = Plate::fromJson(R"({"format":"plate","version":1,"rows":8,"columns":12,"wells":[
            {"well":"I1","type":"sample"},
            {"well":"A1","type":"mystery"},
            {"well":"A1","type":"sample"},
            {"well":"B2","type":"blank","concentration":-1},
            {"well":"C3","type":"control-","label":"ok"}]})", &problems);
        QVERIFY(plate.has_value());
        QCOMPARE(problems.size(), 4);
        QCOMPARE(problems[1].well, QString("A1"));
        QCOMPARE(problems[3].well, QString("B2"));
        QCOMPARE(plate->well(2, 2).type, WellType::ControlNegative);

        // A type that isn't a string is a problem, not a quiet Empty.
        problems.clear();
        QVERIFY(Plate::fromJson(R"({"format":"plate","version":1,"rows":8,"columns":12,"wells":[
            {"well":"A1","type":1,"label":"x"}]})", &problems));
        QCOMPARE(problems.size(), 1);
        QCOMPARE(problems[0].well, QString("A1"));
    }

    void readsTablesAsSpreadsheetsWriteThem()
    {
        // A column copied from a spreadsheet, its header dropped.
        QCOMPARE(Plate::parseTable("Sample\nDonor 01\nDonor 02\n\n"),
                 (QVector<QStringList>{{"Donor 01"}, {"Donor 02"}}));
        // Tabs win over commas inside names.
        QCOMPARE(Plate::parseTable("a, 1\tb\r\nc\td"), (QVector<QStringList>{{"a, 1", "b"}, {"c", "d"}}));
        // CSV quoting, and semicolons where a comma is the decimal separator.
        QCOMPARE(Plate::parseTable("\"Smith, J\",\"say \"\"hi\"\"\"\nx,y"),
                 (QVector<QStringList>{{"Smith, J", "say \"hi\""}, {"x", "y"}}));
        QCOMPARE(Plate::parseTable("a;b,1\n"), (QVector<QStringList>{{"a", "b,1"}}));
        QVERIFY(Plate::parseTable("  \n\t\n").isEmpty());
    }

    void pastesNamesAcrossAndDown()
    {
        Plate plate;
        plate.well(0, 1).type = WellType::Standard;
        int dropped = -1;
        QRect area = plate.pasteLabels(QPoint(0, 0), {{"a", "b"}, {"c", ""}}, WellType::Sample, &dropped);
        QCOMPARE(area, QRect(0, 0, 2, 2));
        QCOMPARE(dropped, 0);
        QCOMPARE(plate.well(0, 1).label, QString("b"));
        QCOMPARE(plate.well(0, 1).type, WellType::Standard); // A typed well keeps its type.
        QCOMPARE(plate.well(1, 0).type, WellType::Sample);   // An empty one takes the fill type.
        QCOMPARE(plate.well(1, 1).type, WellType::Empty);    // A blank cell leaves its well alone.

        // A column of ten names from G1 runs off the bottom of a 96-well plate.
        area = plate.pasteLabels(QPoint(0, 6), {{"1"}, {"2"}, {"3"}, {"4"}}, WellType::Sample, &dropped);
        QCOMPARE(area, QRect(0, 6, 1, 2));
        QCOMPARE(dropped, 2);
        QCOMPARE(plate.labelsAsTable(QRect(0, 0, 2, 2)), QString("a\tb\nc\t"));

        plate.clear(QRect(0, 0, 12, 8));
        QCOMPARE(plate, Plate());
    }

    void backendPastesClearsAndImports()
    {
        FakePicker picker;
        QTemporaryDir dir;
        Backend backend(&picker);
        backend.select(0, 3, 0, 3);
        QVERIFY(backend.pasteText("Donor 01\tDonor 01\nDonor 02\tDonor 02"));
        QCOMPARE(backend.selection(), QRect(3, 0, 2, 2));
        QCOMPARE(backend.plate().well(1, 4).label, QString("Donor 02"));
        QCOMPARE(backend.plate().count(WellType::Sample), 4);
        QVERIFY(!backend.pasteText("\n"));
        QVERIFY(backend.messageIsError());

        backend.clearSelection();
        QCOMPARE(backend.plate(), Plate());
        backend.undo();
        QCOMPARE(backend.plate().count(WellType::Sample), 4);

        // A file of names through the file chooser, its header dropped.
        picker.path = dir.filePath("names.csv");
        QFile file(picker.path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("Sample ID\nP-001\nP-002\nP-003\n");
        file.close();
        backend.select(0, 0, 0, 0);
        backend.importNames();
        QCOMPARE(backend.selectionName(), QString("A1:C1"));
        QCOMPARE(backend.plate().well(2, 0).label, QString("P-003"));
        QVERIFY(!backend.importNamesFrom(dir.filePath("missing.csv")));
    }

    void keepsTemplatesBesideTheBuiltInOnes()
    {
        QTemporaryDir dir;
        // The built-in ELISA layout ships in the binary and is a valid plate.
        const auto builtIn = Templates::list(dir.path());
        QCOMPARE(builtIn.size(), 1);
        QCOMPARE(builtIn[0].name, QString("ELISA"));
        QVERIFY(builtIn[0].builtIn);
        QString error;
        const auto elisa = Templates::load(dir.path(), "ELISA", &error);
        QVERIFY2(elisa.has_value(), qPrintable(error));
        QCOMPARE(elisa->count(WellType::Standard), 16);

        Backend backend;
        backend.setTemplateDirectory(dir.path());
        QSignalSpy changed(&backend, &Backend::templatesChanged);
        backend.setCurrentType(int(WellType::Blank));
        backend.assign(0, 0, 7, 0);
        QVERIFY(backend.saveTemplate("Blanks first"));
        QVERIFY(!backend.saveTemplate("../escape"));
        QVERIFY(!backend.saveTemplate(".hidden"));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(backend.templates().size(), 2);
        QCOMPARE(backend.templates()[1].toMap()["name"].toString(), QString("Blanks first"));

        backend.newPlate(384);
        QVERIFY(backend.newFromTemplate("Blanks first"));
        QCOMPARE(backend.rows(), 8);
        QCOMPARE(backend.plate().count(WellType::Blank), 8);
        QVERIFY(!backend.dirty());
        QVERIFY(backend.filePath().isEmpty());
        QVERIFY(!backend.newFromTemplate("Nothing"));

        // Built-in templates can't be deleted; saved ones can.
        QVERIFY(!backend.deleteTemplate("ELISA"));
        QVERIFY(backend.deleteTemplate("Blanks first"));
        QCOMPARE(backend.templates().size(), 1);
    }

    void writesCsvForInstruments()
    {
        Plate plate;
        plate.well(0, 0) = {WellType::Standard, "Std 1", 100.0};
        plate.well(0, 1) = {WellType::ControlNegative, "Neg, \"buffer\"", std::nullopt};
        plate.well(1, 0) = {WellType::Sample, "S1", 0.125};
        const QStringList lines = QString::fromUtf8(plate.toCsv()).split("\r\n", Qt::SkipEmptyParts);
        QCOMPARE(lines.size(), 97);
        QCOMPARE(lines[0], QString("Well,Type,Label,Concentration"));
        QCOMPARE(lines[1], QString("A1,Standard,Std 1,100"));
        QCOMPARE(lines[2], QString("A2,Control-,\"Neg, \"\"buffer\"\"\","));
        QCOMPARE(lines[3], QString("A3,Empty,,"));
        // Plate order: across row A, then row B.
        QCOMPARE(lines[12], QString("A12,Empty,,"));
        QCOMPARE(lines[13], QString("B1,Sample,S1,0.125"));
        QCOMPARE(lines[96], QString("H12,Empty,,"));
    }

    void exportsAMapAsSvgAndPdf()
    {
        QTemporaryDir dir;
        Plate plate;
        plate.well(0, 0) = {WellType::Standard, "Std 1", 100.0};
        QString error;
        QVERIFY2(exportSvg(plate, dir.filePath("map.svg"), "Assay", QColor("#7aa2f7"), &error), qPrintable(error));
        QVERIFY(readAll(dir.filePath("map.svg")).contains("<svg"));
        QVERIFY(readAll(dir.filePath("map.svg")).contains("Std 1"));
        QVERIFY2(exportPdf(plate, dir.filePath("map.pdf"), "Assay", QColor("#7aa2f7"), &error), qPrintable(error));
        QVERIFY(readAll(dir.filePath("map.pdf")).startsWith("%PDF"));
        QVERIFY(!exportCsv(plate, dir.filePath("missing/dir/out.csv"), &error));
        QVERIFY(!QFile::exists(dir.filePath("missing/dir/out.csv")));
    }

    void colorsEveryTypeApart()
    {
        for (const QString accent : {"#FFD60A", "#7aa2f7", "#9ece6a", "#f7768e", "#888888"}) {
            const PlatePalette palette = screenPalette(QColor(accent));
            QCOMPARE(palette.types[int(WellType::Sample)], QColor(accent));
            for (int a = 0; a < WellTypeCount - 1; ++a)
                for (int b = a + 1; b < WellTypeCount - 1; ++b)
                    QVERIFY2(palette.types[a] != palette.types[b], qPrintable(accent));
        }
    }

    void hitTestsTheWellsItDraws()
    {
        Plate plate;
        const PlateLayout layout = plateLayout(QRectF(0, 0, 1400, 1000), plate);
        QCOMPARE(layout.wellAt(layout.cell(0, 0).center()), QPoint(0, 0));
        QCOMPARE(layout.wellAt(layout.cell(7, 11).center()), QPoint(11, 7));
        QCOMPARE(layout.wellAt(QPointF(1, 1)), QPoint(-1, -1));
    }

    void backendUndoesAndTracksUnsavedWork()
    {
        Backend backend;
        QVERIFY(!backend.dirty());
        backend.setCurrentType(int(WellType::Standard));
        backend.assign(0, 0, 1, 2);
        QCOMPARE(backend.plate().count(WellType::Standard), 6);
        QVERIFY(backend.dirty());
        backend.setSelectionLabel("  Std  ");
        QCOMPARE(backend.plate().well(1, 2).label, QString("Std"));

        backend.undo();
        QCOMPARE(backend.plate().well(1, 2).label, QString());
        backend.undo();
        QCOMPARE(backend.plate().count(WellType::Standard), 0);
        QVERIFY(!backend.dirty());
        QVERIFY(!backend.canUndo());
        backend.redo();
        QCOMPARE(backend.plate().count(WellType::Standard), 6);

        // An edit that changes nothing makes no undo step.
        backend.assignSelection();
        backend.undo();
        QCOMPARE(backend.plate().count(WellType::Standard), 0);
    }

    void backendForgetsASavePointOnADroppedRedoBranch()
    {
        FakePicker picker;
        QTemporaryDir dir;
        picker.path = dir.filePath("saved.plate");
        Backend backend(&picker);
        backend.assign(0, 0, 0, 0);
        backend.assign(0, 1, 0, 1);
        backend.save();
        QVERIFY(!backend.dirty());

        // Undo past the save, then edit as many times: the history index is back
        // where it was saved, but the plate isn't what's on disk.
        backend.undo();
        backend.undo();
        backend.assign(1, 0, 1, 0);
        backend.assign(1, 1, 1, 1);
        QVERIFY(backend.dirty());
        backend.undo();
        backend.undo();
        QVERIFY(backend.dirty());
    }

    void backendDilutesAndReplicatesTheSelection()
    {
        Backend backend;
        backend.setCurrentType(int(WellType::Standard));
        backend.select(0, 0, 0, 3);
        QVERIFY(backend.dilute(64, 4, 0));
        QCOMPARE(*backend.plate().well(0, 3).concentration, 1.0);
        QVERIFY(!backend.dilute(0, 2, 0));
        QVERIFY(backend.messageIsError());

        QVERIFY(backend.replicate(2));
        QCOMPARE(backend.selection(), QRect(0, 1, 4, 1));
        QCOMPARE(*backend.plate().well(1, 2).concentration, 4.0);
        QVERIFY(backend.replicate(2)); // A triplicate, from the copy.
        QCOMPARE(backend.plate().count(WellType::Standard), 12);

        backend.select(0, 8, 7, 11);
        QVERIFY(!backend.replicate(0));
        QVERIFY(backend.messageIsError());
    }

    void backendSavesAtomicallyAndOpens()
    {
        QTemporaryDir dir;
        FakePicker picker;
        Backend backend(&picker);
        backend.newPlate(384);
        backend.setCurrentType(int(WellType::Blank));
        backend.assign(15, 23, 15, 23);

        picker.path = dir.filePath("layout"); // No extension: .plate is added.
        QSignalSpy saved(&backend, &Backend::saved);
        backend.save();
        QCOMPARE(saved.count(), 1);
        QCOMPARE(backend.filePath(), dir.filePath("layout.plate"));
        QVERIFY(!backend.dirty());
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files), QStringList{"layout.plate"});

        picker.path = dir.filePath("layout.csv");
        backend.exportPlate();
        QVERIFY(readAll(dir.filePath("layout.csv")).contains("P24,Blank,,"));

        Backend other(&picker);
        picker.path = dir.filePath("layout.plate");
        other.open();
        QCOMPARE(other.rows(), 16);
        QCOMPARE(other.plate().well(15, 23).type, WellType::Blank);
        QVERIFY(!other.dirty());

        QVERIFY(!other.openPath(dir.filePath("nothing.plate")));
        QVERIFY(other.messageIsError());
        QCOMPARE(other.rows(), 16);
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        QFile colors(dir.path() + "/theme/colors.toml");
        QVERIFY(colors.open(QIODevice::WriteOnly));
        colors.write("accent = \"#112233\"\n");
        colors.close();

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        auto write = [](const QString &path, const QByteArray &data) {
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
        };
        QVERIFY(write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n"));
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QVERIFY(write(response, "{\"int\":0}"));

        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        QVERIFY(write(response, "{\"int\":6}"));
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        write(dir.path() + "/pt.json", R"({"Save": "Guardar", "Open": "Abrir"})");
        write(dir.path() + "/pt-BR.json", R"({"Save": "Salvar"})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("pt-BR"));
        QCOMPARE(translator.translate("Main", "Save"), QString("Salvar"));
        QCOMPARE(translator.translate("Main", "Open"), QString("Abrir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void translatorPicksPluralForms()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "ru\n");
        write(dir.path() + "/en.json", R"({"%n clips": {"one": "%n clip", "other": "%n clips"}})");
        write(dir.path() + "/ru.json", R"({"%n clips": {"one": "%n клип", "few": "%n клипа", "many": "%n клипов", "other": "%n клипа"}})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 21), QString("%n клип"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 22), QString("%n клипа"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 25), QString("%n клипов"));
        QCOMPARE(Translator::pluralCategory("ar", 11), QString("many"));
        QCOMPARE(Translator::pluralCategory("ja", 1), QString("other"));
    }

private:
    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(PlateTests)
#include "plate_tests.moc"
