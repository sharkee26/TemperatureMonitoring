#ifndef TEMPERATURESTORAGE_H
#define TEMPERATURESTORAGE_H

#include <QObject>
#include <QSqlDatabase>
#include <QVariantList>
#include <QString>

// Хранилище измерений (SQLite) и журнала изменений температуры.
class TemperatureStorage : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString unitLabel
                   READ unitLabel
                       NOTIFY snapshotChanged)

    Q_PROPERTY(double currentValue
                   READ currentValue
                       NOTIFY snapshotChanged)

    Q_PROPERTY(QString updatedAt
                   READ updatedAt
                       NOTIFY snapshotChanged)

    Q_PROPERTY(QVariantList changeJournal
                   READ changeJournal
                       NOTIFY journalChanged)

public:
    explicit TemperatureStorage(QObject *parent = nullptr);

    QString unitLabel() const;
    double currentValue() const;
    QString updatedAt() const;
    QVariantList changeJournal() const;

    // Сохраняет новое измерение и при заметном изменении дописывает журнал
    Q_INVOKABLE bool storeReading(double celsius, int rawCode = -1);

signals:
    void snapshotChanged();
    void journalChanged();

private:
    bool buildSchema();
    bool ensureProbe();
    bool seedDemoReadings();
    void refresh();

private:
    QSqlDatabase m_db;

    int m_probeId = -1;

    QString m_unitLabel;
    double m_currentValue = 0.0;
    QString m_updatedAt;

    QVariantList m_changeJournal;
};

#endif // TEMPERATURESTORAGE_H
