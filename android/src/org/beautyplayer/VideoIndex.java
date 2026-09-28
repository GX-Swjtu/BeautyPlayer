package org.beautyplayer;

import android.content.ContentUris;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Environment;
import android.os.CancellationSignal;
import android.provider.DocumentsContract;
import android.provider.MediaStore;
import org.json.JSONArray;
import org.json.JSONObject;
import java.io.File;
import java.util.ArrayDeque;
import java.util.Arrays;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.Locale;
import java.util.Set;
import java.util.Map;
import java.util.HashMap;

/** Fast system index plus explicitly granted directory trees, with no broad storage access. */
public final class VideoIndex {
    private static volatile CancellationSignal activeScan;
    public static void cancel() { CancellationSignal signal = activeScan; if (signal != null) signal.cancel(); }
    private static final String PREFS = "video-directory-grants";
    private static final Set<String> EXTENSIONS = new HashSet<>(Arrays.asList("mp4","mkv","mov","avi","webm","m4v","ts","mts","m2ts","wmv","flv","mpg","mpeg","3gp"));
    public static JSONArray folders(Context context) {
        return new JSONArray(context.getSharedPreferences(PREFS, 0).getStringSet("uris", new HashSet<String>()));
    }
    public static void add(Context context, Uri uri) {
        context.getContentResolver().takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        Set<String> roots = new HashSet<>(context.getSharedPreferences(PREFS, 0).getStringSet("uris", new HashSet<String>()));
        roots.add(uri.toString()); context.getSharedPreferences(PREFS, 0).edit().putStringSet("uris", roots).apply();
    }
    public static void remove(Context context, String value) {
        Set<String> roots = new HashSet<>(context.getSharedPreferences(PREFS, 0).getStringSet("uris", new HashSet<String>()));
        roots.remove(value); context.getSharedPreferences(PREFS, 0).edit().putStringSet("uris", roots).apply();
        try { context.getContentResolver().releasePersistableUriPermission(Uri.parse(value), Intent.FLAG_GRANT_READ_URI_PERMISSION); }
        catch (SecurityException ignored) {}
    }
    public static Uri documentUri(Uri uri) {
        if (DocumentsContract.isTreeUri(uri)) {
            // A tree grant may also be a video document URI: keep its document id.
            try { return DocumentsContract.buildDocumentUriUsingTree(uri, DocumentsContract.getDocumentId(uri)); }
            catch (IllegalArgumentException ignored) { return DocumentsContract.buildDocumentUriUsingTree(uri, DocumentsContract.getTreeDocumentId(uri)); }
        }
        return uri;
    }
    private static String pathKey(Uri tree, String id) {
        if ("com.android.externalstorage.documents".equals(tree.getAuthority()) && id.contains(":")) {
            String[] parts = id.split(":", 2);
            return ("primary".equalsIgnoreCase(parts[0]) ? Environment.getExternalStorageDirectory().getAbsolutePath() : "/storage/" + parts[0]) + "/" + parts[1];
        }
        return DocumentsContract.buildDocumentUriUsingTree(tree, id).toString();
    }
    private static JSONObject entry(String name, String folder, String uri, long bytes, long modified, long created) throws Exception {
        return new JSONObject().put("name", name).put("folder", folder).put("url", uri)
            .put("bytes", bytes).put("modified", modified).put("created", created > 0 ? created : modified);
    }
    private static boolean hasNoMedia(File folder, Map<String, Boolean> known) {
        if (folder == null) return false;
        String path = folder.getAbsolutePath();
        if (known.containsKey(path)) return known.get(path);
        boolean hidden = new File(folder, ".nomedia").isFile() || hasNoMedia(folder.getParentFile(), known);
        known.put(path, hidden); return hidden;
    }
    public static String scan(Context context, boolean respectNoMedia, boolean fullAccess, boolean limited) {
        CancellationSignal signal = new CancellationSignal(); activeScan = signal;
        JSONObject result = new JSONObject(); LinkedHashMap<String, JSONObject> videos = new LinkedHashMap<>();
        int inaccessible = 0;
        Map<String, Boolean> hiddenFolders = new HashMap<>();
        try {
            if (fullAccess || limited) {
                Uri collection = MediaStore.Video.Media.EXTERNAL_CONTENT_URI;
                String[] columns = {"_id", "_display_name", "bucket_display_name", "bucket_id", "_size", "date_modified", "date_added", "_data", "datetaken"};
                try (Cursor cursor = context.getContentResolver().query(collection, columns, "_size>0", null, "date_modified DESC", signal)) {
                    while (cursor != null && cursor.moveToNext() && videos.size() < 100000 && !signal.isCanceled()) {
                        String uri = ContentUris.withAppendedId(collection, cursor.getLong(0)).toString();
                        String path = cursor.getString(7);
                        // Check each directory once to cover newly created markers
                        // while the system index is still catching up. Granted SAF
                        // trees below also enforce the rule without raw-path access.
                        if (respectNoMedia && path != null && path.startsWith("/") && hasNoMedia(new File(path).getParentFile(), hiddenFolders)) continue;
                        String key = path == null || path.isEmpty() ? uri : path;
                        videos.put(key, entry(cursor.getString(1), cursor.getString(3) + "/" + cursor.getString(2), uri,
                            cursor.getLong(4), cursor.getLong(5) * 1000, cursor.getLong(8) > 0 ? cursor.getLong(8) : cursor.getLong(6) * 1000));
                    }
                }
            }
            JSONArray roots = folders(context);
            for (int root = 0; root < roots.length() && !signal.isCanceled(); ++root) {
                Uri tree = Uri.parse(roots.getString(root));
                try {
                    String rootId = DocumentsContract.getTreeDocumentId(tree);
                    LinkedHashMap<String, JSONObject> granted = new LinkedHashMap<>();
                    ArrayDeque<String[]> pending = new ArrayDeque<>();
                    String rootName = rootId.substring(rootId.lastIndexOf(':') + 1);
                    pending.add(new String[]{rootId, rootName});
                    HashSet<String> visited = new HashSet<>();
                    while (!pending.isEmpty() && granted.size() < 100000 && visited.size() < 100000 && !signal.isCanceled()) {
                        String[] directory = pending.removeLast();
                        if (!visited.add(directory[0])) continue;
                        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, directory[0]);
                        String[] projection = {"document_id", "_display_name", "mime_type", "_size", "last_modified"};
                        // One query per folder; first detect .nomedia before queuing children.
                        try (Cursor cursor = context.getContentResolver().query(children, projection, null, null, null, signal)) {
                            if (cursor == null) throw new java.io.IOException("directory unavailable");
                            boolean excluded = false;
                            while (cursor.moveToNext() && !signal.isCanceled()) if (".nomedia".equals(cursor.getString(1)) && respectNoMedia) { excluded = true; break; }
                            if (excluded) continue;
                            cursor.moveToPosition(-1);
                            while (cursor.moveToNext() && granted.size() < 100000 && !signal.isCanceled()) {
                                String id = cursor.getString(0), name = cursor.getString(1), mime = cursor.getString(2);
                                if (DocumentsContract.Document.MIME_TYPE_DIR.equals(mime)) pending.add(new String[]{id, directory[1] + "/" + name});
                                else if (name != null && EXTENSIONS.contains(name.substring(name.lastIndexOf('.') + 1).toLowerCase(Locale.ROOT))) {
                                    String uri = DocumentsContract.buildDocumentUriUsingTree(tree, id).toString();
                                    String key = pathKey(tree, id);
                                    JSONObject indexed = videos.get(key);
                                    long created = indexed != null ? indexed.optLong("created") : cursor.getLong(4);
                                    granted.put(key, entry(name, "目录/" + directory[1], uri, cursor.getLong(3), cursor.getLong(4), created));
                                }
                            }
                        }
                    }
                    String prefix = pathKey(tree, rootId);
                    if (!prefix.endsWith("/")) prefix += "/";
                    final String inside = prefix;
                    // The explicit directory policy wins over stale MediaStore rows.
                    videos.keySet().removeIf(key -> key.startsWith(inside));
                    for (java.util.Map.Entry<String, JSONObject> item : granted.entrySet()) {
                        if (videos.size() >= 100000) break;
                        videos.put(item.getKey(), item.getValue());
                    }
                } catch (Exception error) { ++inaccessible; }
            }
            result.put("videos", new JSONArray(videos.values())); result.put("limited", limited);
            if (!fullAccess && !limited && roots.length() == 0) result.put("error", "允许访问视频或添加扫描文件夹后可查找视频");
            else if (inaccessible > 0) result.put("error", videos.size() + " 个视频 · " + inaccessible + " 个授权目录不可访问，请重新添加");
        } catch (Exception error) {
            try { result.put("videos", new JSONArray(videos.values())); result.put("error", "视频索引读取失败：" + error.getMessage()); } catch (Exception ignored) {}
        }
        return result.toString();
    }
}
