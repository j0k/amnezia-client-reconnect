#include "processRecorderController.h"

#include <QVariantMap>
#include <QSet>

#if defined(Q_OS_WIN)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#endif

namespace {
QString formatDuration(qint64 ms)
{
    if (ms < 0) {
        ms = 0;
    }
    const qint64 total = ms / 1000;
    const qint64 s = total % 60;
    const qint64 m = (total / 60) % 60;
    const qint64 h = total / 3600;
    if (h > 0) {
        return QStringLiteral("%1ч %2м %3с").arg(h).arg(m).arg(s);
    }
    if (m > 0) {
        return QStringLiteral("%1м %2с").arg(m).arg(s);
    }
    return QStringLiteral("%1с").arg(s);
}
}

ProcessRecorderController::ProcessRecorderController(QObject *parent) : QObject(parent)
{
    m_timer.setInterval(m_intervalMs);
    connect(&m_timer, &QTimer::timeout, this, &ProcessRecorderController::captureNow);
}

bool ProcessRecorderController::isRecording() const
{
    return m_recording;
}

int ProcessRecorderController::intervalMs() const
{
    return m_intervalMs;
}

void ProcessRecorderController::setIntervalMs(int ms)
{
    if (ms < 500) {
        ms = 500; // don't hammer the system
    }
    if (ms == m_intervalMs) {
        return;
    }
    m_intervalMs = ms;
    m_timer.setInterval(m_intervalMs);
    emit intervalMsChanged();
}

int ProcessRecorderController::snapshotCount() const
{
    return m_history.size();
}

int ProcessRecorderController::viewIndex() const
{
    return m_viewIndex;
}

void ProcessRecorderController::setViewIndex(int index)
{
    if (m_history.isEmpty()) {
        return;
    }
    index = qBound(0, index, m_history.size() - 1);
    // Scrubbing away from the newest snapshot leaves "live" mode.
    const bool atNewest = (index == m_history.size() - 1);
    if (!atNewest && m_live) {
        m_live = false;
        emit liveChanged();
    }
    if (index == m_viewIndex) {
        return;
    }
    m_viewIndex = index;
    emit viewIndexChanged();
    emit viewChanged();
}

bool ProcessRecorderController::isLive() const
{
    return m_live;
}

void ProcessRecorderController::setLive(bool live)
{
    if (live == m_live) {
        return;
    }
    m_live = live;
    emit liveChanged();
    if (m_live && !m_history.isEmpty()) {
        setViewIndex(m_history.size() - 1);
    }
}

void ProcessRecorderController::jumpToLive()
{
    if (m_history.isEmpty()) {
        return;
    }
    if (!m_live) {
        m_live = true;
        emit liveChanged();
    }
    setViewIndex(m_history.size() - 1);
}

const ProcessRecorderController::Snapshot *ProcessRecorderController::currentSnapshot() const
{
    if (m_viewIndex < 0 || m_viewIndex >= m_history.size()) {
        return nullptr;
    }
    return &m_history.at(m_viewIndex);
}

QString ProcessRecorderController::viewTimestamp() const
{
    const Snapshot *s = currentSnapshot();
    return s ? s->time.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QString();
}

int ProcessRecorderController::viewProcessCount() const
{
    const Snapshot *s = currentSnapshot();
    return s ? s->procs.size() : 0;
}

int ProcessRecorderController::viewNewCount() const
{
    const Snapshot *s = currentSnapshot();
    return s ? s->newCount : 0;
}

QString ProcessRecorderController::filter() const
{
    return m_filter;
}

void ProcessRecorderController::setFilter(const QString &f)
{
    if (f == m_filter) {
        return;
    }
    m_filter = f;
    emit filterChanged();
    emit viewChanged();
}

bool ProcessRecorderController::onlyNew() const
{
    return m_onlyNew;
}

void ProcessRecorderController::setOnlyNew(bool enabled)
{
    if (enabled == m_onlyNew) {
        return;
    }
    m_onlyNew = enabled;
    emit onlyNewChanged();
    emit viewChanged();
}

QVariantList ProcessRecorderController::processes() const
{
    QVariantList list;
    const Snapshot *s = currentSnapshot();
    if (!s) {
        return list;
    }

    const QString needle = m_filter.trimmed();
    for (const ProcInfo &p : s->procs) {
        if (m_onlyNew && !p.isNew) {
            continue;
        }
        if (!needle.isEmpty()
            && !p.name.contains(needle, Qt::CaseInsensitive)
            && !p.path.contains(needle, Qt::CaseInsensitive)
            && QString::number(p.pid) != needle) {
            continue;
        }
        QVariantMap m;
        m["pid"] = p.pid;
        m["ppid"] = p.ppid;
        m["threads"] = p.threads;
        m["name"] = p.name;
        m["path"] = p.path;
        m["startTime"] = p.startTime;
        m["duration"] = p.duration;
        m["isNew"] = p.isNew;
        m["exited"] = p.exited;
        list.append(m);
    }
    return list;
}

void ProcessRecorderController::start()
{
    if (m_recording) {
        return;
    }
    m_recording = true;
    emit recordingChanged();
    m_live = true;
    emit liveChanged();
    captureNow();      // immediate first snapshot
    m_timer.start();
}

void ProcessRecorderController::stop()
{
    if (!m_recording) {
        return;
    }
    m_recording = false;
    m_timer.stop();
    emit recordingChanged();
}

void ProcessRecorderController::toggle()
{
    m_recording ? stop() : start();
}

void ProcessRecorderController::clear()
{
    m_history.clear();
    m_lastPids.clear();
    m_viewIndex = -1;
    emit snapshotsChanged();
    emit viewIndexChanged();
    emit viewChanged();
}

void ProcessRecorderController::captureNow()
{
    Snapshot snap;
    snap.time = QDateTime::currentDateTime();
    const qint64 nowMs = snap.time.toMSecsSinceEpoch();
    snap.procs = enumerateProcesses(); // live processes only at this point

    const Snapshot *prev = m_history.isEmpty() ? nullptr : &m_history.last();

    // Flag processes whose PID was absent in the previous snapshot as "new".
    int newCount = 0;
    if (prev) {
        for (ProcInfo &p : snap.procs) {
            if (!m_lastPids.contains(p.pid)) {
                p.isNew = true;
                ++newCount;
            }
        }
    }
    snap.newCount = newCount;

    // Detect processes that were alive in the previous snapshot but are gone now:
    // append a synthetic "exited" entry carrying the run duration.
    if (prev) {
        QSet<quint32> livePids;
        livePids.reserve(snap.procs.size());
        for (const ProcInfo &p : snap.procs) {
            livePids.insert(p.pid);
        }
        for (const ProcInfo &p : prev->procs) {
            if (p.exited) {
                continue; // don't carry forward prior exit markers
            }
            if (!livePids.contains(p.pid)) {
                ProcInfo e = p;
                e.exited = true;
                e.isNew = false;
                const qint64 startMs = e.startMsecs > 0 ? e.startMsecs : prev->time.toMSecsSinceEpoch();
                e.duration = formatDuration(nowMs - startMs);
                snap.procs.append(e);
            }
        }
    }

    // Remember live PIDs for the next "new" comparison.
    m_lastPids.clear();
    m_lastPids.reserve(snap.procs.size());
    for (const ProcInfo &p : snap.procs) {
        if (!p.exited) {
            m_lastPids.append(p.pid);
        }
    }

    m_history.append(snap);
    if (m_history.size() > m_maxSnapshots) {
        m_history.removeFirst();
        if (m_viewIndex > 0) {
            --m_viewIndex;
        }
    }
    emit snapshotsChanged();

    if (m_live || m_viewIndex < 0) {
        m_viewIndex = m_history.size() - 1;
        emit viewIndexChanged();
    }
    emit viewChanged();
}

QVector<ProcessRecorderController::ProcInfo> ProcessRecorderController::enumerateProcesses() const
{
    QVector<ProcInfo> result;

#if defined(Q_OS_WIN)
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return result;
    }

    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snap, &entry)) {
        do {
            ProcInfo p;
            p.pid = entry.th32ProcessID;
            p.ppid = entry.th32ParentProcessID;
            p.threads = entry.cntThreads;
            p.name = QString::fromWCharArray(entry.szExeFile);

            // Full image path (best effort; needs the process to be openable).
            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
            if (hProc) {
                wchar_t buf[MAX_PATH * 2];
                DWORD sz = static_cast<DWORD>(sizeof(buf) / sizeof(buf[0]));
                if (QueryFullProcessImageNameW(hProc, 0, buf, &sz)) {
                    p.path = QString::fromWCharArray(buf, static_cast<int>(sz));
                }

                FILETIME ftCreate, ftExit, ftKernel, ftUser;
                if (GetProcessTimes(hProc, &ftCreate, &ftExit, &ftKernel, &ftUser)) {
                    ULARGE_INTEGER u;
                    u.LowPart = ftCreate.dwLowDateTime;
                    u.HighPart = ftCreate.dwHighDateTime;
                    // FILETIME: 100ns ticks since 1601-01-01 UTC. 116444736000000000 = ticks 1601->1970.
                    const qint64 msecs = (static_cast<qint64>(u.QuadPart) - 116444736000000000LL) / 10000LL;
                    if (msecs > 0) {
                        p.startMsecs = msecs;
                        p.startTime = QDateTime::fromMSecsSinceEpoch(msecs)
                                          .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
                    }
                }
                CloseHandle(hProc);
            }

            result.append(p);
        } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
#endif

    return result;
}
