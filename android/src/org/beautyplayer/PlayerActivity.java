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
    public static String scanVideos(Context context, boolean respectNoMedia) {
        return VideoIndex.scan(context, respectNoMedia, canReadVideos(context), selectedOnly(context));
    }
    public static String videoFolders(Context context) { return VideoIndex.folders(context).toString(); }
    public static void removeVideoFolder(Context context, String uri) { VideoIndex.remove(context, uri); }
    public static void chooseVideoFolder(Context context) {
        if (!(context instanceof Activity) || permissionPending) return;
        permissionPending = true;
        ((Activity) context).runOnUiThread(() -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION | Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);
            try { ((Activity) context).startActivityForResult(intent, 2027); }
            catch (Exception error) { permissionPending = false; }
        });
    }
    @Override protected void onActivityResult(int request, int result, Intent data) {
        if (request == 2027) {
            try {
                if (result == Activity.RESULT_OK && data != null && data.getData() != null)
                    VideoIndex.add(this, data.getData());
            } catch (Exception error) {
                android.widget.Toast.makeText(this, "无法保存目录权限，请重新选择", android.widget.Toast.LENGTH_LONG).show();
            } finally { permissionPending = false; }
        } else super.onActivityResult(request, result, data);
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
        try (Cursor cursor = context.getContentResolver().query(VideoIndex.documentUri(Uri.parse(value)), new String[]{OpenableColumns.DISPLAY_NAME}, null, null, null)) {
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
