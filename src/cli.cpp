// The command line follows Hype's (MIT, David Heinemeier Hansson).
#include "cli.h"

#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <cstdio>

#include "plate.h"
#include "platepainter.h"
#include "theme.h"

namespace {
void print(FILE *stream, const QString &text) { fprintf(stream, "%s\n", qPrintable(text)); }
void print(const QJsonObject &object) { fprintf(stdout, "%s", QJsonDocument(object).toJson().constData()); }
int fail(const QString &message) {
    print(stderr, message);
    return 1;
}

struct Command {
    QCommandLineParser parser;
    Command(const QString &name, const QString &description, bool file = true) {
        parser.setApplicationDescription(description);
        parser.addHelpOption();
        parser.addPositionalArgument(name, description, name);
        if (file)
            parser.addPositionalArgument("file", "Plate layout (.plate)", "<file>");
    }
    void json() { parser.addOption({"json", "Print the result as JSON"}); }
    QString argument(int index) const { return parser.positionalArguments().value(index); }
};

std::optional<Plate> load(const QString &path, QVector<PlateProblem> *problems) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        problems->append({{}, path + ": " + file.errorString()});
        return std::nullopt;
    }
    return Plate::fromJson(file.readAll(), problems);
}

QJsonObject counts(const Plate &plate) {
    QJsonObject counts;
    for (int i = 0; i < WellTypeCount; ++i)
        counts.insert(Plate::typeId(WellType(i)), plate.count(WellType(i)));
    return counts;
}

int check(const QStringList &arguments) {
    Command command("check", "Validate a plate layout and report every problem.");
    command.json();
    command.parser.process(arguments);
    const QString path = command.argument(1);
    if (path.isEmpty())
        return fail("Name the .plate file to check.");
    QVector<PlateProblem> problems;
    const auto plate = load(path, &problems);
    const bool valid = plate && problems.isEmpty();
    if (command.parser.isSet("json")) {
        QJsonArray list;
        for (const auto &problem : problems)
            list.append(QJsonObject{{"well", problem.well.isEmpty() ? QJsonValue() : QJsonValue(problem.well)},
                                    {"message", problem.message}});
        QJsonObject result{{"file", QFileInfo(path).absoluteFilePath()}, {"valid", valid}, {"problems", list}};
        if (plate) {
            result.insert("rows", plate->rows());
            result.insert("columns", plate->columns());
            result.insert("unit", plate->unit());
            result.insert("counts", counts(*plate));
        }
        print(result);
    } else {
        for (const auto &problem : problems)
            print(stderr, path + ": " + (problem.well.isEmpty() ? QString() : problem.well + ": ") + problem.message);
        if (valid)
            print(stdout, QString("%1: valid %2-well plate").arg(path).arg(plate->wellCount()));
    }
    return valid ? 0 : 1;
}

int exportPlate(const QStringList &arguments) {
    Command command("export", "Export a plate layout as CSV for instruments, or as an SVG or PDF plate map.");
    command.parser.addOption({"csv", "Write Well, Type, Label, Concentration rows to <out>", "out"});
    command.parser.addOption({"svg", "Write the printable plate map as SVG to <out>", "out"});
    command.parser.addOption({"pdf", "Write the printable plate map as PDF to <out>", "out"});
    command.json();
    command.parser.process(arguments);
    const QString path = command.argument(1);
    if (path.isEmpty())
        return fail("Name the .plate file to export.");
    if (!command.parser.isSet("csv") && !command.parser.isSet("svg") && !command.parser.isSet("pdf"))
        return fail("Choose at least one of --csv <out>, --svg <out>, or --pdf <out>.");
    QVector<PlateProblem> problems;
    const auto plate = load(path, &problems);
    if (!plate || !problems.isEmpty()) {
        for (const auto &problem : problems)
            print(stderr, path + ": " + (problem.well.isEmpty() ? QString() : problem.well + ": ") + problem.message);
        return fail(path + " has problems; plate check " + path + " lists them.");
    }

    // The map follows the desktop accent, as the window does.
    const QColor accent(Theme::readAccent(QDir::homePath() + "/.local/state/omarchy/current/theme/colors.toml"));
    const QString title = QFileInfo(path).completeBaseName();
    QJsonArray written;
    for (const QString format : {"csv", "svg", "pdf"}) {
        if (!command.parser.isSet(format))
            continue;
        const QString out = command.parser.value(format);
        QString error;
        const bool ok = format == "csv" ? exportCsv(*plate, out, &error)
                      : format == "svg" ? exportSvg(*plate, out, title, accent, &error)
                                        : exportPdf(*plate, out, title, accent, &error);
        if (!ok)
            return fail("Could not write " + out + ": " + error);
        written.append(QJsonObject{{"format", format}, {"path", QFileInfo(out).absoluteFilePath()}});
        if (!command.parser.isSet("json"))
            print(stdout, "Exported " + out);
    }
    if (command.parser.isSet("json"))
        print({{"file", QFileInfo(path).absoluteFilePath()}, {"exported", written}});
    return 0;
}

int create(const QStringList &arguments) {
    Command command("new", "Start an empty plate layout.");
    command.parser.addOption({"wells", "96 or 384 (default: 96)", "count", "96"});
    command.parser.addOption({"unit", "Concentration unit, such as µM", "unit"});
    command.json();
    command.parser.process(arguments);
    const QString path = command.argument(1);
    if (path.isEmpty())
        return fail("Name the .plate file to create.");
    if (QFileInfo::exists(path))
        return fail(path + " already exists.");
    const QString wells = command.parser.value("wells");
    if (wells != "96" && wells != "384")
        return fail("A plate has 96 or 384 wells, not " + wells + ".");
    Plate plate = wells == "384" ? Plate(16, 24) : Plate(8, 12);
    plate.setUnit(command.parser.value("unit"));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(plate.toJsonBytes()) < 0 || !file.commit())
        return fail(path + ": " + file.errorString());
    if (command.parser.isSet("json"))
        print({{"file", QFileInfo(path).absoluteFilePath()}, {"rows", plate.rows()}, {"columns", plate.columns()}});
    else
        print(stdout, "Created " + path);
    return 0;
}

// As Hype does: one shared copy, linked into Claude Code, found directly by Codex.
int skill(const QStringList &arguments) {
    Command command("skill", "Print the agent skill, or install it for your coding agents with: skill install.", false);
    command.parser.addPositionalArgument("install", "Copy to ~/.agents/skills/plate and link into ~/.claude/skills",
                                         "[install]");
    command.parser.process(arguments);
    QFile bundled(":/skill.md");
    if (!bundled.open(QIODevice::ReadOnly))
        return fail("The bundled skill is missing.");
    const QByteArray text = bundled.readAll();
    if (command.argument(1).isEmpty()) {
        fputs(text.constData(), stdout);
        return 0;
    }
    if (command.argument(1) != "install")
        return fail("Use plate skill to print the skill, or plate skill install.");
    const QDir home = QDir::home();
    const QString directory = home.filePath(".agents/skills/plate");
    QSaveFile file(directory + "/SKILL.md");
    if (!QDir().mkpath(directory) || !file.open(QIODevice::WriteOnly) || file.write(text) < 0 || !file.commit())
        return fail("Could not write " + file.fileName());
    print(stdout, "Installed " + file.fileName());
    if (!home.exists(".claude"))
        return 0;
    const QString link = home.filePath(".claude/skills/plate");
    const QFileInfo existing(link);
    if (existing.isSymLink())
        QFile::remove(link);
    else if (existing.exists())
        return fail(link + " exists and is not a link; leaving it alone.");
    if (!home.mkpath(".claude/skills") || !QFile::link("../../.agents/skills/plate", link))
        return fail("Could not link " + link);
    print(stdout, "Linked " + link);
    return 0;
}

int help(const QStringList &arguments) {
    const QString topic = arguments.value(2);
    if (isCliCommand(topic) && topic != "help")
        return runCli({arguments[0], topic, "--help"});
    print(stdout, "Usage: plate <command> [options]\n\n" + cliSummary());
    return 0;
}
} // namespace

QString cliSummary() {
    return "Window:\n"
           "  open [file]                        Design a plate layout\n\n"
           "Commands that need no display (plate help <command> for options):\n"
           "  new <file> [--wells 96|384]        Start an empty plate layout\n"
           "  check <file> [--json]              Validate a layout and list every problem\n"
           "  export <file> --csv|--svg|--pdf <out> [--json]\n"
           "                                     Export for an instrument, or a printable map\n"
           "  skill [install]                    Print the skill for coding agents, or install it";
}

bool isCliCommand(const QString &word) {
    return QStringList{"new", "check", "export", "skill", "help"}.contains(word);
}

int runCli(const QStringList &arguments) {
    const QString command = arguments.value(1);
    return command == "new"      ? create(arguments)
           : command == "check"  ? check(arguments)
           : command == "export" ? exportPlate(arguments)
           : command == "skill"  ? skill(arguments)
                                 : help(arguments);
}
