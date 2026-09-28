#pragma once
#include <QString>
class QQuickWindow;
class VideoLibrary;
class ThumbnailProvider;
void runLibraryProbe(QQuickWindow *window, VideoLibrary *library, ThumbnailProvider *thumbnails, const QString &prefix);
