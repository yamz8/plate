#pragma once

// The picker interface follows Omacut (MIT, David Heinemeier Hansson), so
// tests can stand in for the desktop portal.

#include <QObject>
#include <QUrl>

class FilePicker : public QObject {
    Q_OBJECT

public:
    explicit FilePicker(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~FilePicker() = default;

    virtual void openPlate() = 0;
    virtual void savePlate(const QUrl &suggestedUrl) = 0;
    // Offers CSV, SVG, and PDF; the chosen filter decides the format when
    // the name has no extension.
    virtual void exportPlate(const QUrl &suggestedUrl) = 0;
    // A CSV or text file of sample names.
    virtual void openNames() = 0;

signals:
    void openSelected(const QUrl &url);
    void namesSelected(const QUrl &url);
    void saveSelected(const QUrl &url);
    // format is "csv", "svg", or "pdf".
    void exportSelected(const QUrl &url, const QString &format);
    void failed(const QString &message);
};
