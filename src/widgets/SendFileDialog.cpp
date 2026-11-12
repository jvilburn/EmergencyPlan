#include "SendFileDialog.h"
#include "ShowInFolder.h"

#include <QApplication>
#include <QDrag>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMimeData>
#include <QMouseEvent>
#include <QPixmap>
#include <QPoint>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace
{

/// The file as its icon beside its name and a hint to drag it. A press and drag carries the
/// file out of the app as the file itself, so it drops into an email or message as an
/// attachment.
class DraggableFile : public QWidget
{
public:
    DraggableFile(const QString& path, QWidget* parent);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    QString m_path;
    QPixmap m_icon;
    QPoint m_pressedAt;
};

DraggableFile::DraggableFile(const QString& path, QWidget* parent)
    : QWidget(parent)
    , m_path(path)
    , m_icon(QFileIconProvider().icon(QFileInfo(path)).pixmap(48, 48))
{
    setCursor(Qt::OpenHandCursor);

    QLabel* icon = new QLabel(this);
    icon->setPixmap(m_icon);
    QLabel* name = new QLabel(QFileInfo(path).fileName(), this);
    name->setTextFormat(Qt::PlainText);
    QLabel* hint = new QLabel(QObject::tr("Drag this into your email"), this);

    QVBoxLayout* words = new QVBoxLayout();
    words->addWidget(name);
    words->addWidget(hint);
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(icon);
    layout->addLayout(words, 1);
}

void DraggableFile::mousePressEvent(QMouseEvent* event)
{
    m_pressedAt = event->position().toPoint();
}

void DraggableFile::mouseMoveEvent(QMouseEvent* event)
{
    if (!(event->buttons() & Qt::LeftButton)
        || (event->position().toPoint() - m_pressedAt).manhattanLength() < QApplication::startDragDistance())
    {
        return;
    }
    QMimeData* mimeData = new QMimeData();
    mimeData->setUrls({QUrl::fromLocalFile(m_path)});
    QDrag* drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->setPixmap(m_icon);
    drag->exec(Qt::CopyAction);
}

}  // namespace

SendFileDialog::SendFileDialog(const QString& title, const QString& text, const QString& path, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(title);

    QVBoxLayout* layout = new QVBoxLayout(this);
    QLabel* textLabel = new QLabel(text, this);
    textLabel->setTextFormat(Qt::PlainText);
    textLabel->setWordWrap(true);
    textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(textLabel);

    m_aboveFile = new QVBoxLayout();
    layout->addLayout(m_aboveFile);
    layout->addWidget(new DraggableFile(path, this));

    QPushButton* showInFolder = new QPushButton(tr("Show in Folder"), this);
    QPushButton* done = new QPushButton(tr("Done"), this);
    done->setDefault(true);
    QHBoxLayout* buttons = new QHBoxLayout();
    buttons->addWidget(showInFolder);
    buttons->addStretch(1);
    buttons->addWidget(done);
    layout->addLayout(buttons);

    connect(showInFolder, &QPushButton::clicked, this, [path]() { ShowInFolder::reveal(path); });
    connect(done, &QPushButton::clicked, this, &QDialog::accept);
}

void SendFileDialog::addAboveFile(QWidget* widget)
{
    m_aboveFile->addWidget(widget);
}
