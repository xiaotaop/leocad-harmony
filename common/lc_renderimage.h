#pragma once

#include <QImage>
#include <QRect>
#include <cstring>

// QOpenGLFramebufferObject::toImage() may return RGBA8888 (GLES) or
// ARGB32 (desktop). Convert before copying bytes into the ARGB32 export image;
// converting the already mislabeled destination cannot recover its channels.
inline void lcCopyRenderImageTile(const QImage& Readback, QImage& Image, const QRect& Source, const QPoint& Target)
{
	Q_ASSERT(Image.format() == QImage::Format_ARGB32);
	Q_ASSERT(Readback.rect().contains(Source));
	Q_ASSERT(Image.rect().contains(QRect(Target, Source.size())));
	const QImage Tile = Readback.convertToFormat(QImage::Format_ARGB32);
	for (int Row = 0; Row < Source.height(); Row++)
		std::memcpy(Image.scanLine(Target.y() + Row) + Target.x() * sizeof(QRgb),
			Tile.constScanLine(Source.y() + Row) + Source.x() * sizeof(QRgb), Source.width() * sizeof(QRgb));
}
