// SPDX-License-Identifier: GPL-3.0-or-later
#include "BarHost.h"

#include "BarCorona.h"
#include "BarLayout.h"
#include "BarWindow.h"
#include "ScreenEdge.h"
#include "ScreenIdentity.h"

#include <Plasma/Containment>

#include <QAction>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QDebug>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScreen>
#include <QSet>
#include <QWindow>

#include <algorithm>

namespace ArchDock::Bars
{
namespace
{
const QString BackendService = QStringLiteral("org.archdock.ArchDock");
const QString BackendPath = QStringLiteral("/Control");
const QString BackendInterface = QStringLiteral("local.PanelWindow");
// How far a bar's window reaches past the bar in edit mode, for its +.
constexpr int EditRoom = 40;
}

BarHost::BarHost(BarCorona *corona, const QUrl &barView, QObject *parent)
    : QObject(parent)
    , m_corona(corona)
    , m_barView(barView)
    , m_recordsFile(qEnvironmentVariable("ARCHDOCK_BARS_RECORDS_FILE"))
    , m_windowsFile(qEnvironmentVariable("ARCHDOCK_BARS_WINDOWS_FILE"))
    , m_edges(new ScreenEdgeManager(this))
    , m_wholeCollections(std::make_unique<QmlGarbageCollector::WholeCollections>())
{
    // Changes come in bursts (a bar's widgets growing one after another, a
    // screen and its geometry): the bars are laid out once after each.
    m_layoutTimer.setSingleShot(true);
    m_layoutTimer.setInterval(0);
    connect(&m_layoutTimer, &QTimer::timeout, this, &BarHost::layoutBars);
    // Whether each bar hides is decided once after a burst of changes too.
    m_evaluateTimer.setSingleShot(true);
    m_evaluateTimer.setInterval(0);
    connect(&m_evaluateTimer, &QTimer::timeout, this, &BarHost::evaluateBars);
    // A widget removed through its own menu leaves the bar's record too, or
    // the next list would put it straight back.
    connect(m_corona, &BarCorona::widgetRemovedByUser, this, [](const QString &barId, const QString &key)
    {
        QDBusMessage call = QDBusMessage::createMethodCall(
            BackendService, BackendPath, BackendInterface, QStringLiteral("barWidgetRemoved"));
        call << barId << key;
        QDBusConnection::sessionBus().asyncCall(call);
    });
    // KWin's auto-hide edges may come after the bars do.
    connect(m_edges, &QWaylandClientExtension::activeChanged, this, &BarHost::scheduleLayout);
    // A popup opening or closing keeps its bar shown or lets it hide.
    qGuiApp->installEventFilter(this);

    const auto screenChanged = [this](QScreen *screen)
    {
        connect(screen, &QScreen::geometryChanged, this, &BarHost::scheduleLayout, Qt::UniqueConnection);
        scheduleLayout();
    };
    for (QScreen *screen : QGuiApplication::screens())
        screenChanged(screen);
    connect(qGuiApp, &QGuiApplication::screenAdded, this, screenChanged);
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, &BarHost::scheduleLayout);
}

BarHost::~BarHost()
{
    qDeleteAll(m_strips);
    qDeleteAll(m_bars);
}

void BarHost::start()
{
    connect(&m_fileWatcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &path)
    {
        if (path == m_recordsFile)
            readRecordsFile();
        else if (path == m_windowsFile)
            readWindowsFile();
    });
    if (!m_recordsFile.isEmpty())
    {
        readRecordsFile();
        if (!m_windowsFile.isEmpty())
            readWindowsFile();
        return;
    }

    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(BackendService, BackendPath, BackendInterface, QStringLiteral("barHostRecordsChanged"),
                this, SLOT(requestRecords()));
    bus.connect(BackendService, BackendPath, BackendInterface, QStringLiteral("barHostWindowsChanged"),
                this, SLOT(requestWindows()));
    // The backend may start after the bars, or start again: each time it
    // comes it is asked for the bars and the windows.
    m_backendWatcher = new QDBusServiceWatcher(BackendService, bus, QDBusServiceWatcher::WatchForRegistration, this);
    connect(m_backendWatcher, &QDBusServiceWatcher::serviceRegistered, this, &BarHost::requestRecords);
    connect(m_backendWatcher, &QDBusServiceWatcher::serviceRegistered, this, &BarHost::requestWindows);
    requestRecords();
    requestWindows();
    // A bar's edit mode is its own, entered and left from the bar's menu:
    // Plasma leaves its own edit mode whenever focus leaves its windows, as
    // a bar's menu takes it, so the bars do not follow it. Entering it from
    // the menu opens Panel Studio for the bar.
    connect(m_corona, &BarCorona::studioRequested, this, [](const QString &barId)
    {
        QDBusMessage show = QDBusMessage::createMethodCall(BackendService, BackendPath, BackendInterface,
                                                           QStringLiteral("showPanelSettings"));
        show << barId;
        QDBusConnection::sessionBus().asyncCall(show);
    });
    connect(m_corona, &BarCorona::widgetDropped, this, &BarHost::addDroppedWidget);
    connect(m_corona, &BarCorona::widgetRemoveRequested, this, [](const QString &barId, const QString &key)
    {
        QDBusMessage remove = QDBusMessage::createMethodCall(BackendService, BackendPath, BackendInterface,
                                                             QStringLiteral("removeBarWidget"));
        remove << barId << key;
        QDBusConnection::sessionBus().asyncCall(remove);
    });
    // The menus' edit mode entries say which way they go, as Plasma's do.
    connect(m_corona, &Plasma::Corona::editModeChanged, this, [this](bool editing)
    {
        const QString text = editing ? tr("Exit Edit Mode") : tr("Enter Edit Mode");
        if (QAction *action = m_corona->action(QStringLiteral("edit mode")))
            action->setText(text);
        for (Plasma::Containment *bar : m_corona->barContainments())
            if (QAction *configure = bar->internalAction(QStringLiteral("configure")))
                configure->setText(text);
        scheduleLayout();
    });
    connect(m_corona, &BarCorona::widgetWindowChanged, this, [this](const QRect &area)
    {
        m_widgetWindow = area;
        scheduleLayout();
    });
    connect(m_corona, &BarCorona::widgetMoved, this, [](const QString &barId, const QString &key, int index)
    {
        QDBusMessage move = QDBusMessage::createMethodCall(BackendService, BackendPath, BackendInterface,
                                                           QStringLiteral("moveBarWidget"));
        move << barId << key << index;
        QDBusConnection::sessionBus().asyncCall(move);
    });
}

void BarHost::addDroppedWidget(const QString &barId, const QString &plugin, int index)
{
    // Arch Dock adds the widget to the bar's record, at its end, and moves
    // it to where it was dropped; the bar then draws it from the record.
    QDBusMessage add = QDBusMessage::createMethodCall(BackendService, BackendPath, BackendInterface,
                                                      QStringLiteral("addBarWidget"));
    add << barId << plugin;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(add), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [barId, plugin, index](QDBusPendingCallWatcher *call)
    {
        call->deleteLater();
        const QDBusPendingReply<QString> reply = *call;
        if (reply.isError() || reply.value().isEmpty())
        {
            qWarning().noquote() << "Arch Dock could not add the widget" << plugin << "to the bar" << barId;
            return;
        }
        QDBusMessage move = QDBusMessage::createMethodCall(BackendService, BackendPath, BackendInterface,
                                                           QStringLiteral("moveBarWidget"));
        move << barId << reply.value() << index;
        QDBusConnection::sessionBus().asyncCall(move);
    });
}

void BarHost::requestRecords()
{
    if (!m_recordsFile.isEmpty())
    {
        readRecordsFile();
        return;
    }
    if (m_fetching)
    {
        m_fetchAgain = true;
        return;
    }
    m_fetching = true;
    const QDBusMessage call = QDBusMessage::createMethodCall(
        BackendService, BackendPath, BackendInterface, QStringLiteral("barHostRecordsJson"));
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *finished)
    {
        finished->deleteLater();
        m_fetching = false;
        const QDBusPendingReply<QString> reply = *finished;
        // Without an answer the bars stay as they are: the backend is not
        // there (yet), and saying nothing is no reason to take bars away.
        if (reply.isError())
            qInfo() << "Arch Dock's backend did not list its bars:" << reply.error().message();
        else
        {
            QString error;
            const QByteArray json = reply.value().toUtf8();
            const QList<BarRecord> records = parseBarRecords(json, &error);
            if (!error.isEmpty())
                qWarning() << "Arch Dock's bar list could not be read:" << error;
            else
                applyRecords(records);
        }
        if (m_fetchAgain)
        {
            m_fetchAgain = false;
            requestRecords();
        }
    });
}

void BarHost::requestWindows()
{
    if (!m_windowsFile.isEmpty())
    {
        readWindowsFile();
        return;
    }
    if (!m_recordsFile.isEmpty())
        return;
    if (m_fetchingWindows)
    {
        m_fetchWindowsAgain = true;
        return;
    }
    m_fetchingWindows = true;
    const QDBusMessage call = QDBusMessage::createMethodCall(
        BackendService, BackendPath, BackendInterface, QStringLiteral("barHostWindowsJson"));
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *finished)
    {
        finished->deleteLater();
        m_fetchingWindows = false;
        const QDBusPendingReply<QString> reply = *finished;
        if (!reply.isError())
            applyWindows(reply.value().toUtf8());
        if (m_fetchWindowsAgain)
        {
            m_fetchWindowsAgain = false;
            requestWindows();
        }
    });
}

void BarHost::readWindowsFile()
{
    if (!m_fileWatcher.files().contains(m_windowsFile))
        m_fileWatcher.addPath(m_windowsFile);
    QFile file(m_windowsFile);
    if (file.open(QIODevice::ReadOnly))
        applyWindows(file.readAll());
}

void BarHost::applyWindows(const QByteArray &json)
{
    // Each window's screen is the one it mostly lies on, so that windows and
    // bars are counted on the same screens.
    const QList<QScreen *> screens = QGuiApplication::screens();
    m_windows.clear();
    QList<QRect> sides;
    const QJsonArray list = QJsonDocument::fromJson(json).array();
    for (const QJsonValue &value : list)
    {
        const QJsonObject object = value.toObject();
        ArchDock::WindowOcclusion window;
        window.frameGeometry = QRect(object.value(QLatin1String("x")).toInt(), object.value(QLatin1String("y")).toInt(),
                                     object.value(QLatin1String("width")).toInt(),
                                     object.value(QLatin1String("height")).toInt());
        if (object.value(QLatin1String("plasmaSide")).toBool())
        {
            sides.append(window.frameGeometry);
            continue;
        }
        window.active = object.value(QLatin1String("active")).toBool();
        window.maximized = object.value(QLatin1String("maximized")).toBool();
        window.fullScreen = object.value(QLatin1String("fullScreen")).toBool();
        qint64 most = 0;
        for (int index = 0; index < screens.size(); ++index)
        {
            const QRect shared = screens.at(index)->geometry().intersected(window.frameGeometry);
            const qint64 area = qint64(shared.width()) * shared.height();
            if (area > most)
            {
                most = area;
                window.screenIndex = index;
            }
        }
        m_windows.append(window);
    }
    if (sides != m_plasmaSides)
    {
        m_plasmaSides = sides;
        scheduleLayout();
    }
    m_evaluateTimer.start();
}

void BarHost::readRecordsFile()
{
    // An editor may write the file anew: it is watched again each time.
    if (!m_fileWatcher.files().contains(m_recordsFile))
        m_fileWatcher.addPath(m_recordsFile);
    QFile file(m_recordsFile);
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "The bar list" << m_recordsFile << "could not be read:" << file.errorString();
        return;
    }
    QString error;
    const QList<BarRecord> records = parseBarRecords(file.readAll(), &error);
    if (!error.isEmpty())
    {
        qWarning() << "The bar list" << m_recordsFile << "could not be read:" << error;
        return;
    }
    applyRecords(records);
}

void BarHost::applyRecords(const QList<BarRecord> &records)
{
    m_records = records;

    QSet<QString> listed;
    for (const BarRecord &record : records)
    {
        listed.insert(record.id);
        Plasma::Containment *containment = m_corona->barContainment(record);
        if (!containment)
            continue;
        BarWindow *&bar = m_bars[record.id];
        if (!bar)
        {
            bar = new BarWindow(m_corona, containment, m_barView);
            // Plasma's shared QML engine is born with the first bar window and
            // has read the collector policy; it leaves the environment before
            // any widget can start a program.
            m_wholeCollections.reset();
            connect(bar, &BarWindow::wantedLengthChanged, this, &BarHost::scheduleLayout);
            connect(bar, &BarWindow::pointerInsideChanged, this, [this] { m_evaluateTimer.start(); });
        }
    }

    // A bar no longer listed is gone from Arch Dock: its window and what it
    // held go with it.
    for (auto it = m_bars.begin(); it != m_bars.end();)
    {
        if (listed.contains(it.key()))
        {
            ++it;
            continue;
        }
        delete it.value();
        it = m_bars.erase(it);
    }
    const QList<Plasma::Containment *> containments = m_corona->barContainments();
    for (Plasma::Containment *containment : containments)
        if (!listed.contains(BarCorona::barId(containment)))
            m_corona->removeBar(containment);

    scheduleLayout();
}

void BarHost::scheduleLayout()
{
    m_layoutTimer.start();
}

QScreen *BarHost::screenFor(const BarRecord &record) const
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    QStringList ids;
    for (const QScreen *screen : screens)
        ids.append(ArchDock::persistentScreenId(screen));
    const int index = ArchDock::resolvedScreenIndex(ids, record.screenId, record.screenIndex);
    return index >= 0 && index < screens.size() ? screens.at(index) : nullptr;
}

void BarHost::layoutBars()
{
    // The shown bars of each screen edge, together.
    struct Edge
    {
        QScreen *screen = nullptr;
        QString edge;
        QList<const BarRecord *> bars;
    };
    QList<Edge> edges;
    for (const BarRecord &record : std::as_const(m_records))
    {
        BarWindow *bar = m_bars.value(record.id);
        if (!bar)
            continue;
        QScreen *screen = record.visible ? screenFor(record) : nullptr;
        if (!screen)
        {
            bar->hide();
            continue;
        }
        auto edge = std::find_if(edges.begin(), edges.end(), [&](const Edge &candidate)
        {
            return candidate.screen == screen && candidate.edge == record.edge;
        });
        if (edge == edges.end())
        {
            edges.append({screen, record.edge, {}});
            edge = std::prev(edges.end());
        }
        edge->bars.append(&record);
    }

    QSet<QString> reserving;
    for (const Edge &edge : std::as_const(edges))
    {
        const QRect area = edge.screen->geometry();
        const bool vertical = edge.edge == QStringLiteral("left") || edge.edge == QStringLiteral("right");
        // Plasma's widget explorer, or any Plasma window standing tall at a
        // side of the screen: a bar along the top or bottom keeps to the part
        // of its edge beside it, shorter if it must be, and a bar on its side
        // stands beside it, as Plasma's own panels do.
        int freeStart = 0, freeEnd = vertical ? area.height() : area.width(), beside = 0;
        QList<QRect> sides = m_plasmaSides;
        if (!m_widgetWindow.isEmpty())
            sides.append(m_widgetWindow);
        for (const QRect &side : std::as_const(sides))
        {
            const QRect shared = side.intersected(area);
            if (shared.isEmpty() || shared.height() < area.height() * 3 / 5)
                continue;
            const bool atLeft = shared.left() <= area.left() + 2;
            const bool atRight = shared.right() >= area.right() - 2;
            if (!vertical && atLeft)
                freeStart = std::max(freeStart, shared.right() + 1 - area.left());
            else if (!vertical && atRight)
                freeEnd = std::min(freeEnd, shared.left() - area.left());
            else if ((edge.edge == QStringLiteral("left") && atLeft) || (edge.edge == QStringLiteral("right") && atRight))
                beside = std::max(beside, shared.width());
        }
        const int edgeLength = std::max(0, freeEnd - freeStart);
        QList<BarRequest> requests;
        for (const BarRecord *record : edge.bars)
        {
            // A bar set to a length is at least as long as what it holds, so
            // nothing in it is squeezed or cut off.
            const auto *window = m_bars.value(record->id);
            const int wanted = window ? window->wantedLength() : 0;
            int length = std::max(record->fixedLength, std::min(wanted, edgeLength));
            if (record->lengthMode == QStringLiteral("fill"))
                length = edgeLength;
            else if (record->lengthMode == QStringLiteral("fit"))
                length = std::clamp(wanted, record->minimumLength, record->maximumLength);
            requests.append({record->id, record->alignment, record->order, length, record->offset});
        }

        int depth = 0;
        const QList<BarPlacement> placements = placeOnEdge(requests, edgeLength);
        QList<BarPlacement> along = placements;
        std::sort(along.begin(), along.end(), [](const BarPlacement &a, const BarPlacement &b) { return a.start < b.start; });
        for (const BarPlacement &placement : placements)
        {
            const auto record = std::find_if(edge.bars.cbegin(), edge.bars.cend(),
                                             [&](const BarRecord *bar) { return bar->id == placement.id; });
            if (record == edge.bars.cend())
                continue;
            // Edit mode: room for the bar's + outside it, past its end, or
            // past its start where the edge has none after it.
            int roomBefore = 0, roomAfter = 0;
            if (m_corona->isEditMode())
            {
                const auto at = std::find(along.cbegin(), along.cend(), placement);
                const int previousEnd = at == along.cbegin() ? 0 : std::prev(at)->start + std::prev(at)->length;
                const int nextStart = std::next(at) == along.cend() ? edgeLength : std::next(at)->start;
                if (nextStart - (placement.start + placement.length) >= EditRoom)
                    roomAfter = EditRoom;
                else if (placement.start - previousEnd >= EditRoom)
                    roomBefore = EditRoom;
            }
            BarWindow *bar = m_bars.value(placement.id);
            bar->place(edge.screen, edge.edge, freeStart + placement.start, placement.length, (*record)->thickness,
                       (*record)->floatingMargin + beside, roomBefore, roomAfter);
            bar->setHides(ArchDock::panelVisibilityModeFromString((*record)->visibilityMode)
                              != ArchDock::PanelVisibilityMode::AlwaysVisible,
                          m_edges);
            if ((*record)->reservesSpace && placement.length > 0)
                depth = std::max(depth, (*record)->thickness + (*record)->floatingMargin);
        }

        if (depth <= 0)
            continue;
        const QString key = edge.screen->name() + QLatin1Char('/') + edge.edge;
        ReservedStrip *&strip = m_strips[key];
        if (strip && strip->placedScreen() != edge.screen)
        {
            delete strip;
            strip = nullptr;
        }
        if (!strip)
            strip = new ReservedStrip(edge.screen, edge.edge);
        strip->setDepth(depth);
        reserving.insert(key);
    }

    for (auto it = m_strips.begin(); it != m_strips.end();)
    {
        if (reserving.contains(it.key()))
        {
            ++it;
            continue;
        }
        delete it.value();
        it = m_strips.erase(it);
    }
    evaluateBars();
}

void BarHost::evaluateBars()
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (const BarRecord &record : std::as_const(m_records))
    {
        BarWindow *bar = m_bars.value(record.id);
        if (!bar || !bar->isVisible())
            continue;
        ArchDock::PanelVisibilityInput input;
        input.mode = ArchDock::panelVisibilityModeFromString(record.visibilityMode);
        input.panelGeometry = bar->geometry();
        input.panelScreenIndex = int(screens.indexOf(bar->screen()));
        input.windows = m_windows;
        input.dodgeAnyWindow = true;
        input.locks.pointerInside = bar->pointerInside();
        input.locks.popupOpen = popupOpen(bar);
        const bool conceal = ArchDock::decidePanelVisibility(input) == ArchDock::PanelVisibilityDecision::Conceal;
        qCDebug(archdockBars) << record.id << record.visibilityMode << "conceal" << conceal << "pointer"
                              << input.locks.pointerInside << "popup" << input.locks.popupOpen << "windows"
                              << m_windows.size();
        bar->setConcealWanted(conceal);
    }
}

bool BarHost::popupOpen(const BarWindow *bar) const
{
    const QWindowList windows = QGuiApplication::topLevelWindows();
    for (const QWindow *window : windows)
    {
        if (window == bar || !window->isVisible())
            continue;
        for (const QWindow *parent = window->transientParent(); parent; parent = parent->transientParent())
            if (parent == bar)
                return true;
    }
    return false;
}

bool BarHost::eventFilter(QObject *watched, QEvent *event)
{
    if ((event->type() == QEvent::Show || event->type() == QEvent::Hide) && watched->isWindowType())
        m_evaluateTimer.start();
    return QObject::eventFilter(watched, event);
}
}
