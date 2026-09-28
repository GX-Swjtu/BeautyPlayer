package org.beautyplayer;

import android.content.Context;
import android.graphics.Bitmap;
import android.media.MediaMetadataRetriever;
import android.net.Uri;
import android.os.Build;
import android.util.Size;
import java.io.ByteArrayOutputStream;

/** Invoked only on the bounded thumbnail worker, never the Activity/UI thread. */
public final class MediaThumbnails {
    private static boolean blank(Bitmap bitmap) {
        if (bitmap == null) return true;
        if (bitmap.getConfig() == Bitmap.Config.HARDWARE) return false;
        int min = 255, max = 0;
        for (int y = 1; y < 9; ++y) for (int x = 1; x < 9; ++x) {
            int pixel = bitmap.getPixel(x * bitmap.getWidth() / 10, y * bitmap.getHeight() / 10);
            int light = (((pixel >> 16) & 255) + ((pixel >> 8) & 255) + (pixel & 255)) / 3;
            min = Math.min(min, light); max = Math.max(max, light);
        }
        return max < 8 || min > 247;
    }
    public static byte[] load(Context context, String value) {
        Bitmap bitmap = null;
        try {
            Uri uri = Uri.parse(value);
            if (Build.VERSION.SDK_INT >= 29 && "content".equals(uri.getScheme())) {
                try { bitmap = context.getContentResolver().loadThumbnail(uri, new Size(512, 384), null); }
                catch (Exception ignored) { /* SAF providers need not implement thumbnails. */ }
            }
            if (blank(bitmap)) {
                try (MediaMetadataRetriever retriever = new MediaMetadataRetriever()) {
                    if ("file".equals(uri.getScheme())) retriever.setDataSource(uri.getPath());
                    else retriever.setDataSource(context, uri);
                    long duration = 0;
                    try { duration = Long.parseLong(retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_DURATION)); } catch (Exception ignored) {}
                    long timeUs = duration > 0 ? Math.min(Math.max(0, duration - 100), Math.min(10000, duration / 10) + 137) * 1000 : -1;
                    Bitmap extracted = retriever.getScaledFrameAtTime(timeUs, MediaMetadataRetriever.OPTION_CLOSEST, 512, 384);
                    if (extracted != null) { if (bitmap != null) bitmap.recycle(); bitmap = extracted; }
                } catch (Exception ignored) { /* Keep the system frame if fallback is unsupported. */ }
            }
            if (bitmap == null) return null;
            ByteArrayOutputStream bytes = new ByteArrayOutputStream();
            bitmap.compress(Bitmap.CompressFormat.JPEG, 82, bytes);
            return bytes.toByteArray();
        } catch (Exception error) { return null; }
        finally { if (bitmap != null) bitmap.recycle(); }
    }
}
