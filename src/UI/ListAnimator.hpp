#pragma once

#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QPersistentModelIndex>
#include <QPixmap>
#include <QRect>
#include <QTimer>
#include <QUrl>

#include "Core/AnimatedImageCache.hpp"
#include "Core/Snowflake.hpp"
#include "UI/Chat/FrameAnimator.hpp"

class QAbstractItemView;

namespace Acheron {
namespace UI {

// Plays animated avatars and server icons in a list or tree view (#119).
// Only what was painted animates, and on hover only the row under the mouse,
// so an idle list costs nothing. Delegates call frame() and fall back to the
// still image when it returns a null pixmap.
class ListAnimator : public QObject
{
    Q_OBJECT
public:
    ListAnimator(QAbstractItemView *view, Core::AnimatedImageCache *cache);

    // the animator installed on the view a delegate is painting for, if any
    [[nodiscard]] static ListAnimator *forWidget(const QWidget *widget);

    void setMode(AvatarAnimation mode);

    [[nodiscard]] QPixmap frame(const QModelIndex &index, const QUrl &animatedUrl, const QSize &logicalSize,
                                Core::Snowflake accountId, const QRect &rect);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct Painted
    {
        QUrl url;
        QSize size;
        QRect rect;
        Core::AnimatedFramesPtr frames;
        int shownFrame = -1;
    };

    void setHovered(const QModelIndex &index);
    void schedule();
    void onTick();
    void onFramesReady(const QUrl &url, const QSize &size, const Core::AnimatedFramesPtr &frames);
    [[nodiscard]] bool active() const;

    QAbstractItemView *view;
    Core::AnimatedImageCache *cache;
    AvatarAnimation mode;
    QPersistentModelIndex hovered;
    QList<Painted> painted;
    QElapsedTimer clock;
    QTimer timer;
};

} // namespace UI
} // namespace Acheron
