// Adapted from Omacut's portal file picker (MIT, David Heinemeier Hansson).
#include "portalfilepicker.h"

#include <QCoreApplication>

#include <QDBusConnection>
#include <QDBusArgument>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QStandardPaths>

namespace {
struct PortalFilterRule {
    uint type;
    QString pattern;
};

using PortalFilterRules = QList<PortalFilterRule>;

struct PortalFileFilter {
    QString name;
    PortalFilterRules rules;
};

using PortalFileFilters = QList<PortalFileFilter>;

QDBusArgument &operator<<(QDBusArgument &argument, const PortalFilterRule &rule) {
    argument.beginStructure();
    argument << rule.type << rule.pattern;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, PortalFilterRule &rule) {
    argument.beginStructure();
    argument >> rule.type >> rule.pattern;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const PortalFileFilter &filter) {
    argument.beginStructure();
    argument << filter.name << filter.rules;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, PortalFileFilter &filter) {
    argument.beginStructure();
    argument >> filter.name >> filter.rules;
    argument.endStructure();
    return argument;
}

void registerPortalFilterTypes() {
    static const bool registered = [] {
        qDBusRegisterMetaType<PortalFilterRule>();
        qDBusRegisterMetaType<PortalFilterRules>();
        qDBusRegisterMetaType<PortalFileFilter>();
        qDBusRegisterMetaType<PortalFileFilters>();
        return true;
    }();
    Q_UNUSED(registered);
}

// Filter names show in the chooser, so they follow the language too.
QString filterName(const char *name) {
    return QCoreApplication::translate("PortalFilePicker", name);
}

PortalFileFilter plateFilter() {
    return {filterName("Plate layouts"), {{0, QStringLiteral("*.plate")}}};
}

PortalFileFilters plateFilters() {
    return {plateFilter(), {filterName("All files"), {{0, QStringLiteral("*")}}}};
}

PortalFileFilter namesFilter() {
    return {filterName("Sample names (CSV or text)"), {{0, QStringLiteral("*.csv")}, {0, QStringLiteral("*.tsv")}, {0, QStringLiteral("*.txt")}}};
}

// The export formats, in the order the dialog offers them.
QList<QPair<QString, PortalFileFilter>> exportFilters() {
    return {
        {QStringLiteral("csv"), {filterName("CSV for plate readers and liquid handlers"), {{0, QStringLiteral("*.csv")}}}},
        {QStringLiteral("svg"), {filterName("SVG plate map"), {{0, QStringLiteral("*.svg")}}}},
        {QStringLiteral("pdf"), {filterName("PDF plate map"), {{0, QStringLiteral("*.pdf")}}}},
    };
}

QString portalToken() {
    return QStringLiteral("plate_%1").arg(QRandomGenerator::global()->generate());
}

QByteArray portalPathBytes(const QString &path) {
    QByteArray bytes = path.toUtf8();
    bytes.append('\0');
    return bytes;
}

QString openFolder() {
    const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return QDir(documents).exists() ? documents : QDir::homePath();
}
}

PortalFilePicker::PortalFilePicker(QObject *parent) : FilePicker(parent) {
    registerPortalFilterTypes();
}

void PortalFilePicker::openPlate() {
    QVariantMap options;
    options.insert(QStringLiteral("accept_label"), tr("Open"));
    options.insert(QStringLiteral("modal"), true);
    options.insert(QStringLiteral("multiple"), false);
    options.insert(QStringLiteral("current_folder"), portalPathBytes(openFolder()));
    options.insert(QStringLiteral("filters"), QVariant::fromValue(plateFilters()));
    options.insert(QStringLiteral("current_filter"), QVariant::fromValue(plateFilter()));
    requestFile(QStringLiteral("OpenFile"), tr("Open Plate Layout"), options, Action::Open);
}

void PortalFilePicker::savePlate(const QUrl &suggestedUrl) {
    const QFileInfo target(suggestedUrl.toLocalFile());
    QVariantMap options;
    options.insert(QStringLiteral("accept_label"), tr("Save"));
    options.insert(QStringLiteral("modal"), true);
    options.insert(QStringLiteral("current_folder"), portalPathBytes(target.absolutePath()));
    options.insert(QStringLiteral("current_name"), target.fileName());
    options.insert(QStringLiteral("filters"), QVariant::fromValue(PortalFileFilters{plateFilter()}));
    options.insert(QStringLiteral("current_filter"), QVariant::fromValue(plateFilter()));
    requestFile(QStringLiteral("SaveFile"), tr("Save Plate Layout"), options, Action::Save);
}

void PortalFilePicker::exportPlate(const QUrl &suggestedUrl) {
    const QFileInfo target(suggestedUrl.toLocalFile());
    PortalFileFilters filters;
    for (const auto &entry : exportFilters())
        filters.append(entry.second);
    QVariantMap options;
    options.insert(QStringLiteral("accept_label"), tr("Export"));
    options.insert(QStringLiteral("modal"), true);
    options.insert(QStringLiteral("current_folder"), portalPathBytes(target.absolutePath()));
    options.insert(QStringLiteral("current_name"), target.fileName());
    options.insert(QStringLiteral("filters"), QVariant::fromValue(filters));
    options.insert(QStringLiteral("current_filter"), QVariant::fromValue(filters.first()));
    requestFile(QStringLiteral("SaveFile"), tr("Export Plate"), options, Action::Export);
}

void PortalFilePicker::openNames() {
    QVariantMap options;
    options.insert(QStringLiteral("accept_label"), tr("Import"));
    options.insert(QStringLiteral("modal"), true);
    options.insert(QStringLiteral("multiple"), false);
    options.insert(QStringLiteral("current_folder"), portalPathBytes(openFolder()));
    options.insert(QStringLiteral("filters"), QVariant::fromValue(PortalFileFilters{
        namesFilter(), {filterName("All files"), {{0, QStringLiteral("*")}}}}));
    options.insert(QStringLiteral("current_filter"), QVariant::fromValue(namesFilter()));
    requestFile(QStringLiteral("OpenFile"), tr("Import Sample Names"), options, Action::Names);
}

bool PortalFilePicker::connectToRequestPath(const QString &path) {
    m_pendingPath = path;
    return QDBusConnection::sessionBus().connect(
        QStringLiteral("org.freedesktop.portal.Desktop"), m_pendingPath,
        QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
        this, SLOT(handleResponse(uint,QVariantMap)));
}

void PortalFilePicker::closePendingRequest() {
    if (m_pendingPath.isEmpty())
        return;
    QDBusInterface request(QStringLiteral("org.freedesktop.portal.Desktop"), m_pendingPath,
                           QStringLiteral("org.freedesktop.portal.Request"),
                           QDBusConnection::sessionBus());
    request.asyncCall(QStringLiteral("Close"));
}

void PortalFilePicker::requestFile(const QString &method, const QString &title,
                                   QVariantMap options, Action action) {
    // A request whose Response never arrives (the portal crashed or was
    // killed with the dialog up) must not wedge every later dialog behind
    // it: close the stuck one best-effort and replace it.
    if (m_pendingAction != Action::None) {
        closePendingRequest();
        clearPending();
    }

    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusInterface portal(QStringLiteral("org.freedesktop.portal.Desktop"),
                          QStringLiteral("/org/freedesktop/portal/desktop"),
                          QStringLiteral("org.freedesktop.portal.FileChooser"),
                          bus);
    if (!portal.isValid()) {
        emit failed(tr("The desktop file chooser (xdg-desktop-portal) is not available."));
        return;
    }

    // Subscribe to the Response signal at the request path the portal will
    // derive from our handle_token *before* making the call, so a response
    // can't slip past while our match rule is still being installed.
    const QString token = portalToken();
    options.insert(QStringLiteral("handle_token"), token);
    QString sender = bus.baseService().mid(1);
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));
    const QString predictedPath =
        QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);

    m_pendingAction = action;
    if (!connectToRequestPath(predictedPath)) {
        clearPending();
        emit failed(tr("Could not listen for the file chooser's answer."));
        return;
    }

    const int generation = ++m_pendingGeneration;
    auto *watcher = new QDBusPendingCallWatcher(
        portal.asyncCall(method, QString(), title, options), this);
    m_pendingWatcher = watcher;

    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, generation]() {
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        watcher->deleteLater();
        if (m_pendingWatcher == watcher)
            m_pendingWatcher = nullptr;

        // Superseded by a newer request, or a response may already have
        // been handled while the reply was in flight.
        if (generation != m_pendingGeneration || m_pendingAction == Action::None)
            return;

        if (reply.isError()) {
            clearPending();
            emit failed(tr("The file chooser failed: %1")
                            .arg(reply.error().message()));
            return;
        }

        // Old portal versions can hand back a different request path than the
        // handle_token predicts; move our subscription over if so.
        const QString actualPath = reply.value().path();
        if (actualPath != m_pendingPath) {
            QDBusConnection::sessionBus().disconnect(
                QStringLiteral("org.freedesktop.portal.Desktop"), m_pendingPath,
                QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
                this, SLOT(handleResponse(uint,QVariantMap)));
            if (!connectToRequestPath(actualPath)) {
                clearPending();
                emit failed(tr("Could not listen for the file chooser's answer."));
            }
        }
    });
}

void PortalFilePicker::handleResponse(uint response, const QVariantMap &results) {
    const Action action = m_pendingAction;
    clearPending();

    if (response != 0)
        return;

    const QStringList uris = results.value(QStringLiteral("uris")).toStringList();
    if (uris.isEmpty())
        return;

    const QUrl url(uris.first());
    if (action == Action::Open) {
        emit openSelected(url);
        return;
    }
    if (action == Action::Names) {
        emit namesSelected(url);
        return;
    }
    if (action == Action::Save) {
        emit saveSelected(url);
        return;
    }
    if (action != Action::Export)
        return;

    // A typed extension wins; otherwise the filter the person left selected.
    QString format = QFileInfo(url.toLocalFile()).suffix().toLower();
    bool known = false;
    for (const auto &entry : exportFilters())
        known = known || entry.first == format;
    if (!known) {
        format = exportFilters().first().first;
        const QVariant current = results.value(QStringLiteral("current_filter"));
        if (current.canConvert<QDBusArgument>()) {
            PortalFileFilter filter;
            current.value<QDBusArgument>() >> filter;
            for (const auto &entry : exportFilters())
                if (entry.second.name == filter.name)
                    format = entry.first;
        }
    }
    emit exportSelected(url, format);
}

void PortalFilePicker::clearPending() {
    // A caller that never gets its reply must not outlive the request it
    // belonged to (repeated stuck requests would otherwise pile them up).
    if (m_pendingWatcher) {
        m_pendingWatcher->deleteLater();
        m_pendingWatcher = nullptr;
    }
    if (!m_pendingPath.isEmpty()) {
        QDBusConnection::sessionBus().disconnect(
            QStringLiteral("org.freedesktop.portal.Desktop"), m_pendingPath,
            QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
            this, SLOT(handleResponse(uint,QVariantMap)));
    }

    m_pendingPath.clear();
    m_pendingAction = Action::None;
}
