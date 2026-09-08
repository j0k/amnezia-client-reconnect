#ifndef PROCESSRECORDERCONTROLLER_H
#define PROCESSRECORDERCONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVector>
#include <QString>
#include <QDateTime>

// Records a snapshot of every running process on a fixed interval while "recording",
// keeps a bounded history, and lets the UI show the live list or scrub back through
// time with a slider. Processes that appeared since the previous snapshot are flagged
// as "new" so the user can see exactly what got launched.
class ProcessRecorderController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool recording READ isRecording NOTIFY recordingChanged)
    Q_PROPERTY(int intervalMs READ intervalMs WRITE setIntervalMs NOTIFY intervalMsChanged)
    Q_PROPERTY(int snapshotCount READ snapshotCount NOTIFY snapshotsChanged)
    Q_PROPERTY(int viewIndex READ viewIndex WRITE setViewIndex NOTIFY viewIndexChanged)
    Q_PROPERTY(bool live READ isLive WRITE setLive NOTIFY liveChanged)
    Q_PROPERTY(QString viewTimestamp READ viewTimestamp NOTIFY viewChanged)
    Q_PROPERTY(int viewProcessCount READ viewProcessCount NOTIFY viewChanged)
    Q_PROPERTY(int viewNewCount READ viewNewCount NOTIFY viewChanged)
    Q_PROPERTY(QVariantList processes READ processes NOTIFY viewChanged)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(bool onlyNew READ onlyNew WRITE setOnlyNew NOTIFY onlyNewChanged)

public:
    explicit ProcessRecorderController(QObject *parent = nullptr);

    bool isRecording() const;

    int intervalMs() const;
    void setIntervalMs(int ms);

    int snapshotCount() const;

    int viewIndex() const;
    void setViewIndex(int index);

    bool isLive() const;
    void setLive(bool live);

    QString viewTimestamp() const;
    int viewProcessCount() const;
    int viewNewCount() const;
    QVariantList processes() const;

    QString filter() const;
    void setFilter(const QString &f);

    bool onlyNew() const;
    void setOnlyNew(bool enabled);

public slots:
    void start();
    void stop();
    void toggle();
    void clear();
    void jumpToLive();
    // Captures one snapshot immediately (also used by the timer).
    void captureNow();

signals:
    void recordingChanged();
    void intervalMsChanged();
    void snapshotsChanged();
    void viewIndexChanged();
    void liveChanged();
    void viewChanged();
    void filterChanged();
    void onlyNewChanged();

private:
    struct ProcInfo {
        quint32 pid = 0;
        quint32 ppid = 0;
        quint32 threads = 0;
        QString name;
        QString path;
        QString startTime;    // process creation time, formatted; empty if unavailable
        qint64 startMsecs = 0; // creation time as epoch ms (0 if unknown)
        QString duration;     // run duration, filled only for exited processes
        bool isNew = false;
        bool exited = false;  // true for a synthetic entry marking a process that just exited
    };
    struct Snapshot {
        QDateTime time;
        QVector<ProcInfo> procs;
        int newCount = 0;
    };

    QVector<ProcInfo> enumerateProcesses() const;
    const Snapshot *currentSnapshot() const;

    QTimer m_timer;
    bool m_recording = false;
    int m_intervalMs = 2000;

    QVector<Snapshot> m_history;
    int m_maxSnapshots = 900; // ~30 min at 2 s

    int m_viewIndex = -1;
    bool m_live = true; // when true, the view follows the newest snapshot
    QString m_filter;
    bool m_onlyNew = false;

    QVector<quint32> m_lastPids; // to compute "new" between consecutive snapshots
};

#endif // PROCESSRECORDERCONTROLLER_H
