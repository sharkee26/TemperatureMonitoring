#include "temperaturestorage.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QVariantMap>

namespace {
const QString kConnectionName = QStringLiteral("tempmonitor_connection");
const QString kDatabaseFile   = QStringLiteral("tempmonitor.db");
const QString kProbeCode      = QStringLiteral("TM-01");

// Минимальное изменение (°C), которое попадает в журнал
const double kJournalStep = 0.2;
const double kEpsilon     = 0.000001;

// Сколько записей журнала отдаём в интерфейс
const int kJournalLimit = 50;
}

TemperatureStorage::TemperatureStorage(QObject *parent)
    : QObject(parent)
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    QDir().mkpath(dir);

    const QString dbPath = dir + QLatin1Char('/') + kDatabaseFile;

    qDebug() << "Файл базы данных:" << dbPath;

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), kConnectionName);
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qDebug() << "Не удалось открыть базу:" << m_db.lastError().text();
        return;
    }

    QSqlQuery(m_db).exec(QStringLiteral("PRAGMA foreign_keys = ON"));

    buildSchema();
    ensureProbe();
    seedDemoReadings();
    refresh();
}

bool TemperatureStorage::buildSchema()
{
    QSqlQuery q(m_db);

    const QStringList statements = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS probes ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "probe_code TEXT UNIQUE NOT NULL,"
            "kind TEXT,"
            "note TEXT,"
            "unit_label TEXT,"
            "added_at TEXT)"),

        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS readings ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "probe_id INTEGER NOT NULL,"
            "taken_at TEXT NOT NULL,"
            "celsius REAL NOT NULL,"
            "raw_code INTEGER,"
            "FOREIGN KEY(probe_id) REFERENCES probes(id))"),

        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS change_journal ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "probe_id INTEGER NOT NULL,"
            "switched_at TEXT NOT NULL,"
            "previous_c REAL NOT NULL,"
            "current_c REAL NOT NULL,"
            "FOREIGN KEY(probe_id) REFERENCES probes(id))")
    };

    for (const QString &sql : statements) {
        if (!q.exec(sql)) {
            qDebug() << "Ошибка создания таблицы:" << q.lastError().text();
            return false;
        }
    }

    return true;
}

bool TemperatureStorage::ensureProbe()
{
    QSqlQuery q(m_db);

    q.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO probes "
        "(probe_code, kind, note, unit_label, added_at) "
        "VALUES (?, ?, ?, ?, ?)"));

    q.addBindValue(kProbeCode);
    q.addBindValue(QStringLiteral("LM35"));
    q.addBindValue(QStringLiteral("Датчик температуры на Arduino"));
    q.addBindValue(QStringLiteral("°C"));
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));

    if (!q.exec()) {
        qDebug() << q.lastError().text();
        return false;
    }

    q.prepare(QStringLiteral("SELECT id FROM probes WHERE probe_code = ?"));
    q.addBindValue(kProbeCode);

    if (!q.exec() || !q.next())
        return false;

    m_probeId = q.value(0).toInt();

    return true;
}

bool TemperatureStorage::seedDemoReadings()
{
    QSqlQuery q(m_db);

    q.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM readings WHERE probe_id = ?"));
    q.addBindValue(m_probeId);
    q.exec();

    if (q.next() && q.value(0).toInt() > 0)
        return true;

    // Первый запуск: небольшой набор значений, чтобы журнал не был пустым
    struct Sample { int secondsAgo; double celsius; int rawCode; };

    const Sample samples[] = {
        {60, 24.5, 50},
        {40, 24.7, 51},
        {20, 24.5, 50},
        { 0, 24.9, 51}
    };

    const QDateTime now = QDateTime::currentDateTime();

    double previous = 0.0;
    bool havePrevious = false;

    for (const Sample &s : samples) {
        const QString stamp = now.addSecs(-s.secondsAgo).toString(Qt::ISODate);

        QSqlQuery ins(m_db);

        ins.prepare(QStringLiteral(
            "INSERT INTO readings (probe_id, taken_at, celsius, raw_code) "
            "VALUES (?, ?, ?, ?)"));
        ins.addBindValue(m_probeId);
        ins.addBindValue(stamp);
        ins.addBindValue(s.celsius);
        ins.addBindValue(s.rawCode);

        if (!ins.exec()) {
            qDebug() << ins.lastError().text();
            return false;
        }

        if (havePrevious && !qFuzzyCompare(previous + 1.0, s.celsius + 1.0)) {
            QSqlQuery jr(m_db);

            jr.prepare(QStringLiteral(
                "INSERT INTO change_journal "
                "(probe_id, switched_at, previous_c, current_c) "
                "VALUES (?, ?, ?, ?)"));
            jr.addBindValue(m_probeId);
            jr.addBindValue(stamp);
            jr.addBindValue(previous);
            jr.addBindValue(s.celsius);

            if (!jr.exec()) {
                qDebug() << jr.lastError().text();
                return false;
            }
        }

        previous = s.celsius;
        havePrevious = true;
    }

    return true;
}

void TemperatureStorage::refresh()
{
    QSqlQuery q(m_db);

    q.prepare(QStringLiteral(
        "SELECT id, unit_label FROM probes WHERE probe_code = ?"));
    q.addBindValue(kProbeCode);

    if (q.exec() && q.next()) {
        m_probeId = q.value(0).toInt();
        m_unitLabel = q.value(1).toString();
    }

    q.prepare(QStringLiteral(
        "SELECT celsius, taken_at FROM readings "
        "WHERE probe_id = ? ORDER BY id DESC LIMIT 1"));
    q.addBindValue(m_probeId);

    if (q.exec() && q.next()) {
        m_currentValue = q.value(0).toDouble();
        m_updatedAt = q.value(1).toString();
    }

    QVariantList journal;

    q.prepare(QStringLiteral(
        "SELECT switched_at, previous_c, current_c FROM change_journal "
        "WHERE probe_id = ? ORDER BY id DESC LIMIT %1").arg(kJournalLimit));
    q.addBindValue(m_probeId);

    if (q.exec()) {
        while (q.next()) {
            const double from = q.value(1).toDouble();
            const double to = q.value(2).toDouble();

            QVariantMap entry;
            entry[QStringLiteral("stamp")]  = q.value(0).toString();
            entry[QStringLiteral("before")] = from;
            entry[QStringLiteral("after")]  = to;
            entry[QStringLiteral("shift")]  = to - from;

            journal.append(entry);
        }
    }

    emit snapshotChanged();

    // Журнал обновляем только при реальных изменениях, иначе список
    // на странице журнала сбрасывался бы каждую секунду.
    if (journal != m_changeJournal) {
        m_changeJournal = journal;
        emit journalChanged();
    }
}

bool TemperatureStorage::storeReading(double celsius, int rawCode)
{
    // Опорное значение: последняя запись журнала, а если её нет -
    // самое первое измерение.
    double reference = 0.0;
    bool haveReference = false;

    QSqlQuery refQuery(m_db);

    refQuery.prepare(QStringLiteral(
        "SELECT current_c FROM change_journal "
        "WHERE probe_id = ? ORDER BY id DESC LIMIT 1"));
    refQuery.addBindValue(m_probeId);

    if (refQuery.exec() && refQuery.next()) {
        reference = refQuery.value(0).toDouble();
        haveReference = true;
    }

    if (!haveReference) {
        QSqlQuery firstQuery(m_db);

        firstQuery.prepare(QStringLiteral(
            "SELECT celsius FROM readings "
            "WHERE probe_id = ? ORDER BY id ASC LIMIT 1"));
        firstQuery.addBindValue(m_probeId);

        if (firstQuery.exec() && firstQuery.next()) {
            reference = firstQuery.value(0).toDouble();
            haveReference = true;
        }
    }

    const QString stamp = QDateTime::currentDateTime().toString(Qt::ISODate);

    if (!m_db.transaction())
        return false;

    QSqlQuery ins(m_db);

    ins.prepare(QStringLiteral(
        "INSERT INTO readings (probe_id, taken_at, celsius, raw_code) "
        "VALUES (?, ?, ?, ?)"));
    ins.addBindValue(m_probeId);
    ins.addBindValue(stamp);
    ins.addBindValue(celsius);
    ins.addBindValue(rawCode);

    if (!ins.exec()) {
        qDebug() << "Ошибка записи измерения:" << ins.lastError().text();
        m_db.rollback();
        return false;
    }

    if (haveReference && qAbs(celsius - reference) + kEpsilon >= kJournalStep) {
        QSqlQuery jr(m_db);

        jr.prepare(QStringLiteral(
            "INSERT INTO change_journal "
            "(probe_id, switched_at, previous_c, current_c) "
            "VALUES (?, ?, ?, ?)"));
        jr.addBindValue(m_probeId);
        jr.addBindValue(stamp);
        jr.addBindValue(reference);
        jr.addBindValue(celsius);

        if (!jr.exec()) {
            qDebug() << "Ошибка записи в журнал:" << jr.lastError().text();
            m_db.rollback();
            return false;
        }
    }

    if (!m_db.commit())
        return false;

    refresh();

    return true;
}

QString TemperatureStorage::unitLabel() const
{
    return m_unitLabel;
}

double TemperatureStorage::currentValue() const
{
    return m_currentValue;
}

QString TemperatureStorage::updatedAt() const
{
    return m_updatedAt;
}

QVariantList TemperatureStorage::changeJournal() const
{
    return m_changeJournal;
}
