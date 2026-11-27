#include "UI/ListAnimator.hpp"

#include <QAbstractItemView>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPaintEvent>

#include <limits>

namespace Acheron {
namespace UI {

ListAnimator::ListAnimator(QAbstractItemView *itemView, Core::AnimatedImageCache *animatedCache)
    : QObject(itemView), view(itemView), cache(animatedCache), mode(avatarAnimationSetting())
{
    clock.start();
    timer.setSingleShot(true);
    timer.setTimerType(Qt::CoarseTimer);
    connect(&timer, &QTimer::timeout, this, &ListAnimator::onTick);
    connect(cache, &Core::AnimatedImageCache::framesReady, this, &ListAnimator::onFramesReady);
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this]() { schedule(); });

    view->viewport()->setMouseTracking(true);
    view->viewport()->installEventFilter(this);
}

ListAnimator *ListAnimator::forWidget(const QWidget *widget)
{
    return widget ? widget->findChild<ListAnimator *>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
}

void ListAnimator::setMode(AvatarAnimation value)
{
    if (mode == value)
        return;
    mode = value;
    painted.clear();
    timer.stop();
    view->viewport()->update();
}

QPixmap ListAnimator::frame(const QModelIndex &index, const QUrl &animatedUrl, const QSize &logicalSize,
                            Core::Snowflake accountId, const QRect &rect)
{
    if (!animatedUrl.isValid() || mode == AvatarAnimation::Never)
        return {};
    if (mode == AvatarAnimation::OnHover && (!hovered.isValid() || hovered != index))
        return {};

    const Core::AnimatedFramesPtr frames = cache->get(animatedUrl, logicalSize, accountId);
    if (frames && (frames->isStatic() || frames->loopMs() <= 0))
        return {};

    Painted entry{ animatedUrl, logicalSize, rect, frames, -1 };
    if (frames)
        entry.shownFrame = frames->frameAt(clock.elapsed() % frames->loopMs());
    painted.append(entry);
    schedule();
    return frames ? frames->frames[entry.shownFrame] : QPixmap();
}

bool ListAnimator::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::Paint: {
        // whatever is repainted records itself again, so drop what this paint covers
        const QRegion region = static_cast<QPaintEvent *>(event)->region();
        for (auto it = painted.begin(); it != painted.end();)
            it = region.intersects(it->rect) ? painted.erase(it) : it + 1;
        break;
    }
    case QEvent::MouseMove:
        if (mode == AvatarAnimation::OnHover)
            setHovered(view->indexAt(static_cast<QMouseEvent *>(event)->pos()));
        break;
    case QEvent::Leave:
        setHovered({});
        break;
    case QEvent::Hide:
        painted.clear();
        timer.stop();
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

void ListAnimator::setHovered(const QModelIndex &index)
{
    if (hovered == index)
        return;
    if (hovered.isValid())
        view->viewport()->update(view->visualRect(hovered));
    hovered = index;
    if (hovered.isValid())
        view->viewport()->update(view->visualRect(hovered));
}

bool ListAnimator::active() const
{
    return !painted.isEmpty() && view->isVisible() && qGuiApp->applicationState() == Qt::ApplicationActive;
}

void ListAnimator::schedule()
{
    if (!active()) {
        timer.stop();
        return;
    }
    const qint64 now = clock.elapsed();
    qint64 wait = std::numeric_limits<qint64>::max();
    for (const Painted &p : std::as_const(painted)) {
        if (!p.frames)
            continue;
        const qint64 phase = now % p.frames->loopMs();
        const int index = p.frames->frameAt(phase);
        wait = qMin<qint64>(wait, p.frames->frameEndMs[index] - phase);
    }
    if (wait == std::numeric_limits<qint64>::max())
        return; // still decoding; framesReady repaints
    const int delay = int(qBound(qint64(15), wait, qint64(1000)));
    if (!timer.isActive() || timer.remainingTime() > delay)
        timer.start(delay);
}

void ListAnimator::onTick()
{
    const qint64 now = clock.elapsed();
    QRegion dirty;
    for (const Painted &p : std::as_const(painted)) {
        if (p.frames && p.frames->frameAt(now % p.frames->loopMs()) != p.shownFrame)
            dirty += p.rect;
    }
    if (!dirty.isEmpty())
        view->viewport()->update(dirty);
    schedule();
}

void ListAnimator::onFramesReady(const QUrl &url, const QSize &size, const Core::AnimatedFramesPtr &)
{
    for (const Painted &p : std::as_const(painted)) {
        if (!p.frames && p.url == url && p.size == size)
            view->viewport()->update(p.rect);
    }
}

} // namespace UI
} // namespace Acheron
