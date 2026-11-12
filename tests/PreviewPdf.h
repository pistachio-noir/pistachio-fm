#pragma once

#include <QPainter>
#include <QPdfWriter>

// A real two-page PDF, shared by backend and window checks; no external generator needed.
inline bool makePreviewPdf(const QString &path)
{
    QPdfWriter writer(path);
    writer.setResolution(96);
    QPainter painter(&writer);
    if (!painter.isActive())
        return false;
    painter.fillRect(0, 0, writer.width(), writer.height(), Qt::white);
    QFont font;
    font.setPixelSize(36);
    painter.setFont(font);
    painter.drawText(QRect(0, 0, writer.width(), writer.height()), Qt::AlignCenter, QStringLiteral("Page one"));
    if (!writer.newPage())
        return false;
    painter.fillRect(0, 0, writer.width(), writer.height(), QColor(232, 242, 255));
    painter.drawText(QRect(0, 0, writer.width(), writer.height()), Qt::AlignCenter, QStringLiteral("Page two"));
    return painter.end();
}
