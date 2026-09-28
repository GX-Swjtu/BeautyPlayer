package org.beautyplayer;

import android.Manifest;
import android.app.Activity;
import android.content.ContentUris;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.ParcelFileDescriptor;
import android.provider.MediaStore;
import android.provider.OpenableColumns;
import android.view.WindowManager;
import org.json.JSONArray;
import org.json.JSONObject;
import org.qtproject.qt.android.bindings.QtActivity;

public class PlayerActivity extends QtActivity {
    public static volatile boolean permissionPending = false;
    private static final int VIDEO_PERMISSION = 2026;

    private static boolean canReadVideos(Context context) {
        String permission = Build.VERSION.SDK_INT >= 33 ? Manifest.permission.READ_MEDIA_VIDEO : Manifest.permission.READ_EXTERNAL_STORAGE;
        return context.checkSelfPermission(permission) == PackageManager.PERMISSION_GRANTED;
    }
    private static boolean selectedOnly(Context context) {
        return Build.VERSION.SDK_INT >= 34 && !canReadVideos(context)
            && context.checkSelfPermission(Manifest.permission.READ_MEDIA_VISUAL_USER_SELECTED) == PackageManager.PERMISSION_GRANTED;
    }
    public static void requestVideoAccess(Context context) {
        if (!(context instanceof Activity) || permissionPending || canReadVideos(context)) return;
        permissionPending = true;
        Activity activity = (Activity) context;
        activity.runOnUiThread(() -> {
            String[] permissions = Build.VERSION.SDK_INT >= 34
                ? new String[]{Manifest.permission.READ_MEDIA_VIDEO, Manifest.permission.READ_MEDIA_VISUAL_USER_SELECTED}
                : new String[]{Build.VERSION.SDK_INT >= 33 ? Manifest.permission.READ_MEDIA_VIDEO : Manifest.permission.READ_EXTERNAL_STORAGE};
            activity.requestPermissions(permissions, VIDEO_PERMISSION);
        });
    }
    @Override public void onRequestPermissionsResult(int request, String[] permissions, int[] results) {
        if (request == VIDEO_PERMISSION) permissionPending = false;
        else super.onRequestPermissionsResult(request, permissions, results);
    }
    // Query the OS index once. No recursive walks, media decoding or thumbnails.
    public static String scanVideos(Context context) {
        JSONObject result = new JSONObject();
        JSONArray videos = new JSONArray();
        try {
            boolean limited = selectedOnly(context);
            if (!canReadVideos(context) && !limited) {
                result.put("error", "允许访问视频后可自动扫描；也可点“文件”单独选择视频");
            } else {
                Uri collection = MediaStore.Video.Media.EXTERNAL_CONTENT_URI;
                String[] columns = {MediaStore.Video.Media._ID, MediaStore.Video.Media.DISPLAY_NAME,
                    MediaStore.Video.Media.BUCKET_DISPLAY_NAME, MediaStore.Video.Media.BUCKET_ID,
                    MediaStore.Video.Media.SIZE, MediaStore.Video.Media.DATE_MODIFIED};
                try (Cursor cursor = context.getContentResolver().query(collection, columns,
                        MediaStore.Video.Media.SIZE + ">0", null, MediaStore.Video.Media.DATE_MODIFIED + " DESC")) {
                    while (cursor != null && cursor.moveToNext() && videos.length() < 100000) {
                        JSONObject video = new JSONObject();
                        video.put("name", cursor.getString(1));
                        video.put("folder", cursor.getString(3) + "/" + cursor.getString(2));
                        video.put("url", ContentUris.withAppendedId(collection, cursor.getLong(0)).toString());
                        video.put("bytes", cursor.getLong(4)); video.put("modified", cursor.getLong(5) * 1000);
                        videos.put(video);
                    }
                }
                result.put("limited", limited);
            }
            result.put("videos", videos);
        } catch (Exception error) {
            try { result.put("error", "视频索引读取失败：" + error.getMessage()); } catch (Exception ignored) {}
        }
        return result.toString();
    }
    public static void retainUri(Context context, String value) {
        try { context.getContentResolver().takePersistableUriPermission(Uri.parse(value), Intent.FLAG_GRANT_READ_URI_PERMISSION); }
        catch (SecurityException ignored) { /* MediaStore grants are managed by the runtime permission. */ }
    }
    public static int openReadFd(Context context, String value) {
        try (ParcelFileDescriptor descriptor = context.getContentResolver().openFileDescriptor(Uri.parse(value), "r")) {
            return descriptor == null ? -1 : descriptor.detachFd();
        } catch (Exception error) { return -1; }
    }
    public static String displayName(Context context, String value) {
        try (Cursor cursor = context.getContentResolver().query(Uri.parse(value), new String[]{OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) return cursor.getString(0);
        } catch (Exception ignored) {}
        return "";
    }
    public static void keepScreenOn(Context context, boolean enabled) {
        if (context instanceof Activity) {
            Activity activity = (Activity) context;
            activity.runOnUiThread(() -> {
                if (enabled) activity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                else activity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            });
        }
    }
}
