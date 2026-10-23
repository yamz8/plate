#pragma once

#include <QColor>
#include <QObject>
#include <QRect>
#include <QUrl>
#include <QVariantMap>
#include <QVector>

#include "plate.h"

class FilePicker;

// The open plate layout and everything done to it, exposed to QML as backend.
// Every change goes through commit(), which keeps undo and redo and knows
// whether there is unsaved work.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(int rows READ rows NOTIFY plateChanged)
    Q_PROPERTY(int columns READ columns NOTIFY plateChanged)
    Q_PROPERTY(int revision READ revision NOTIFY plateChanged)
    Q_PROPERTY(QString unit READ unit NOTIFY plateChanged)
    Q_PROPERTY(QVariantList typeCounts READ typeCounts NOTIFY plateChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY plateChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY plateChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY plateChanged)
    Q_PROPERTY(int currentType READ currentType WRITE setCurrentType NOTIFY currentTypeChanged)
    Q_PROPERTY(int cursorRow READ cursorRow NOTIFY selectionChanged)
    Q_PROPERTY(int cursorColumn READ cursorColumn NOTIFY selectionChanged)
    Q_PROPERTY(QRect selection READ selection NOTIFY selectionChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY fileChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY fileChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(bool messageIsError READ messageIsError NOTIFY messageChanged)

public:
    explicit Backend(FilePicker *picker = nullptr, QObject *parent = nullptr);

    const Plate &plate() const { return m_history[m_index]; }
    int rows() const { return plate().rows(); }
    int columns() const { return plate().columns(); }
    int revision() const { return m_revision; }
    QString unit() const { return plate().unit(); }
    QVariantList typeCounts() const;
    bool canUndo() const { return m_index > 0; }
    bool canRedo() const { return m_index + 1 < m_history.size(); }
    bool dirty() const { return m_index != m_cleanIndex; }
    int currentType() const { return int(m_currentType); }
    void setCurrentType(int type);
    int cursorRow() const { return m_cursor.y(); }
    int cursorColumn() const { return m_cursor.x(); }
    QRect selection() const { return m_selection; }
    QString filePath() const { return m_filePath; }
    QString fileName() const;
    QString message() const { return m_message; }
    bool messageIsError() const { return m_messageIsError; }
    QColor accent() const { return m_accent; }
    void setAccent(const QColor &accent) { m_accent = accent; }

    Q_INVOKABLE QString typeName(int type) const;
    Q_INVOKABLE QString wellName(int row, int column) const { return Plate::wellName(row, column); }
    Q_INVOKABLE QString selectionName() const { return Plate::rangeName(m_selection); }
    // name, type, typeName, label, and concentration (absent when unset).
    Q_INVOKABLE QVariantMap wellInfo(int row, int column) const;
    // The selection's shared label and concentration, and whether they differ.
    Q_INVOKABLE QVariantMap selectionInfo() const;

    Q_INVOKABLE void newPlate(int wells);
    Q_INVOKABLE void setCursor(int row, int column, bool extend = false);
    Q_INVOKABLE void moveCursor(int rowStep, int columnStep, bool extend = false);
    Q_INVOKABLE void select(int row1, int column1, int row2, int column2);
    // Assigns the current type to every well in the rectangle and selects it.
    Q_INVOKABLE void assign(int row1, int column1, int row2, int column2);
    Q_INVOKABLE void assignSelection();
    Q_INVOKABLE void setSelectionLabel(const QString &label);
    // A number sets it; anything else (undefined, an empty string) clears it.
    Q_INVOKABLE void setSelectionConcentration(const QVariant &value);
    Q_INVOKABLE void setUnit(const QString &unit);
    // direction: 0 right, 1 left, 2 down, 3 up.
    Q_INVOKABLE bool dilute(double start, double factor, int direction);
    Q_INVOKABLE bool replicate(int direction);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

    Q_INVOKABLE bool openPath(const QString &path);
    Q_INVOKABLE bool savePath(const QString &path);
    Q_INVOKABLE bool exportTo(const QString &path, const QString &format);
    // These go through the desktop file chooser when there's no path yet.
    Q_INVOKABLE void open();
    Q_INVOKABLE void save();
    Q_INVOKABLE void saveAs();
    Q_INVOKABLE void exportPlate();
    Q_INVOKABLE void clearMessage();

signals:
    void plateChanged();
    void currentTypeChanged();
    void selectionChanged();
    void fileChanged();
    void messageChanged();
    // After a save lands on disk, so a pending quit or new plate can go on.
    void saved();

private:
    void commit(const Plate &plate);
    void resetHistory(const Plate &plate);
    void say(const QString &message, bool error = false);
    QString suggestedPath(const QString &extension) const;

    FilePicker *m_picker = nullptr;
    QVector<Plate> m_history;
    int m_index = 0;
    int m_cleanIndex = 0;
    int m_revision = 0;
    WellType m_currentType = WellType::Sample;
    QPoint m_cursor{0, 0};
    QPoint m_anchor{0, 0};
    QRect m_selection{0, 0, 1, 1};
    QString m_filePath;
    QString m_message;
    bool m_messageIsError = false;
    QColor m_accent{"#FFD60A"};
};
