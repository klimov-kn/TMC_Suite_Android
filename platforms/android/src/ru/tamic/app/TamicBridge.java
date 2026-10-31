// TamicBridge — Java-помощник оболочки TAMIC (вызывается из C++ через QJniObject,
// src/app/platform_android.cpp).
//
//  * папка задач, выбранная системным окном (SAF, ACTION_OPEN_DOCUMENT_TREE) с
//    постоянным разрешением (решение M16): список файлов, чтение, копирование во
//    внутреннюю рабочую папку и обратно — ядру нужны обычные пути и fopen (A8);
//  * «Поделиться» результатами (ACTION_SEND / ACTION_SEND_MULTIPLE);
//  * экстры интента запуска (режим самопроверки: am start -e autotest …);
//  * служба расчёта (CalcService) и разрешение на уведомления (Android 13+).
package ru.tamic.app;

import android.Manifest;
import android.app.Activity;
import android.content.ClipData;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.UriPermission;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Document;
import android.util.Log;

import androidx.core.content.FileProvider;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;

public final class TamicBridge {
    private static final String TAG = "TAMIC";
    private static final char SEP = '\u001f';
    private static final int REQ_NOTIFICATIONS = 4712;

    private TamicBridge() {}

    /** Кнопка «Остановить» в уведомлении службы — в C++ (Platform::stopRequested). */
    public static native void nativeStopRequested();

    // ------------------------------------------------------------ каталоги, запуск

    public static String nativeLibraryDir(Context ctx) {
        return ctx.getApplicationInfo().nativeLibraryDir;
    }

    public static String externalFilesDir(Context ctx) {
        File f = ctx.getExternalFilesDir(null);
        return f != null ? f.getAbsolutePath() : "";
    }

    public static String launchExtra(Context ctx, String key) {
        if (!(ctx instanceof Activity)) return "";
        Intent it = ((Activity) ctx).getIntent();
        if (it == null) return "";
        String v = it.getStringExtra(key);
        return v != null ? v : "";
    }

    /**
     * Проверочный запуск (am start с экстрами autotest/screen): нажимать на телефоне
     * нельзя, поэтому окно показывается поверх экрана блокировки, включает экран и не
     * даёт ему погаснуть, пока приложение открыто. Обычный запуск этого не делает.
     */
    public static void prepareForTest(Context ctx) {
        if (!(ctx instanceof Activity)) return;
        final Activity a = (Activity) ctx;
        a.runOnUiThread(() -> {
            a.setShowWhenLocked(true);
            a.setTurnScreenOn(true);
            a.getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        });
    }

    // ------------------------------------------------------------ выбор папки (SAF)

    public static Intent folderPickIntent() {
        Intent it = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        it.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                | Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION
                | Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);
        return it;
    }

    /** Ответ окна выбора: закрепить разрешение навсегда, вернуть URI папки. */
    public static String onFolderPicked(Context ctx, Intent data) {
        if (data == null || data.getData() == null) return "";
        Uri uri = data.getData();
        try {
            ctx.getContentResolver().takePersistableUriPermission(uri,
                    Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        } catch (SecurityException e) {
            Log.w(TAG, "takePersistableUriPermission: " + e);
        }
        return uri.toString();
    }

    public static boolean hasAccess(Context ctx, String uriStr) {
        for (UriPermission p : ctx.getContentResolver().getPersistedUriPermissions()) {
            if (p.getUri().toString().equals(uriStr) && p.isReadPermission() && p.isWritePermission())
                return true;
        }
        return false;
    }

    public static void releaseAccess(Context ctx, String uriStr) {
        try {
            ctx.getContentResolver().releasePersistableUriPermission(Uri.parse(uriStr),
                    Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        } catch (SecurityException e) {
            Log.w(TAG, "releasePersistableUriPermission: " + e);
        }
    }

    private static Uri treeDocUri(Uri tree) {
        return DocumentsContract.buildDocumentUriUsingTree(tree, DocumentsContract.getTreeDocumentId(tree));
    }

    public static String folderTitle(Context ctx, String uriStr) {
        Uri tree = Uri.parse(uriStr);
        try (Cursor c = ctx.getContentResolver().query(treeDocUri(tree),
                new String[] { Document.COLUMN_DISPLAY_NAME }, null, null, null)) {
            if (c != null && c.moveToFirst() && c.getString(0) != null) return c.getString(0);
        } catch (Exception e) {
            Log.w(TAG, "folderTitle: " + e);
        }
        String id = DocumentsContract.getTreeDocumentId(tree);
        int k = Math.max(id.lastIndexOf('/'), id.lastIndexOf(':'));
        return k >= 0 && k + 1 < id.length() ? id.substring(k + 1) : id;
    }

    private static final String[] COLS = {
        Document.COLUMN_DOCUMENT_ID, Document.COLUMN_DISPLAY_NAME, Document.COLUMN_SIZE,
        Document.COLUMN_LAST_MODIFIED, Document.COLUMN_MIME_TYPE
    };

    /** Файлы папки: «имя␟размер␟мс␟каталог(0/1)». null — нет доступа. */
    public static String[] listFolder(Context ctx, String uriStr) {
        Uri tree = Uri.parse(uriStr);
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree,
                DocumentsContract.getTreeDocumentId(tree));
        ArrayList<String> out = new ArrayList<>();
        try (Cursor c = ctx.getContentResolver().query(children, COLS, null, null, null)) {
            while (c != null && c.moveToNext()) {
                String name = c.getString(1);
                long size = c.isNull(2) ? 0 : c.getLong(2);
                long mod = c.isNull(3) ? 0 : c.getLong(3);
                boolean dir = Document.MIME_TYPE_DIR.equals(c.getString(4));
                out.add(name + SEP + size + SEP + mod + SEP + (dir ? "1" : "0"));
            }
        } catch (Exception e) {
            Log.w(TAG, "listFolder: " + e);
            return null;
        }
        return out.toArray(new String[0]);
    }

    private static Uri findChild(ContentResolver cr, Uri tree, String name) {
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree,
                DocumentsContract.getTreeDocumentId(tree));
        try (Cursor c = cr.query(children, COLS, null, null, null)) {
            while (c != null && c.moveToNext()) {
                if (name.equals(c.getString(1)))
                    return DocumentsContract.buildDocumentUriUsingTree(tree, c.getString(0));
            }
        } catch (Exception e) {
            Log.w(TAG, "findChild: " + e);
        }
        return null;
    }

    private static void copy(InputStream in, OutputStream out) throws IOException {
        byte[] buf = new byte[1 << 16];
        int n;
        while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
    }

    public static byte[] readFile(Context ctx, String uriStr, String name, long maxBytes) {
        ContentResolver cr = ctx.getContentResolver();
        Uri doc = findChild(cr, Uri.parse(uriStr), name);
        if (doc == null) return null;
        try (InputStream in = cr.openInputStream(doc)) {
            if (in == null) return null;
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] buf = new byte[1 << 16];
            int n;
            long total = 0;
            while ((n = in.read(buf)) > 0 && total < maxBytes) {
                int take = (int) Math.min(n, maxBytes - total);
                out.write(buf, 0, take);
                total += take;
            }
            return out.toByteArray();
        } catch (Exception e) {
            Log.w(TAG, "readFile " + name + ": " + e);
            return null;
        }
    }

    public static boolean copyToLocal(Context ctx, String uriStr, String name, String localPath) {
        ContentResolver cr = ctx.getContentResolver();
        Uri doc = findChild(cr, Uri.parse(uriStr), name);
        if (doc == null) return false;
        try (InputStream in = cr.openInputStream(doc);
             OutputStream out = new FileOutputStream(localPath)) {
            if (in == null) return false;
            copy(in, out);
            return true;
        } catch (Exception e) {
            Log.w(TAG, "copyToLocal " + name + ": " + e);
            return false;
        }
    }

    public static boolean copyFromLocal(Context ctx, String localPath, String uriStr, String name) {
        ContentResolver cr = ctx.getContentResolver();
        Uri tree = Uri.parse(uriStr);
        try {
            Uri doc = findChild(cr, tree, name);
            if (doc == null)
                doc = DocumentsContract.createDocument(cr, treeDocUri(tree), "application/octet-stream", name);
            if (doc == null) return false;
            OutputStream out;
            try {
                out = cr.openOutputStream(doc, "wt");   // с усечением старого файла
            } catch (Exception e) {
                out = cr.openOutputStream(doc, "w");
            }
            if (out == null) return false;
            try (InputStream in = new FileInputStream(localPath); OutputStream o = out) {
                copy(in, o);
            }
            return true;
        } catch (Exception e) {
            Log.w(TAG, "copyFromLocal " + name + ": " + e);
            return false;
        }
    }

    /** Удалить файл папки SAF (результат расчёта по просьбе пользователя). */
    public static boolean deleteFile(Context ctx, String uriStr, String name) {
        ContentResolver cr = ctx.getContentResolver();
        Uri doc = findChild(cr, Uri.parse(uriStr), name);
        if (doc == null) return true;   // файла уже нет
        try {
            return DocumentsContract.deleteDocument(cr, doc);
        } catch (Exception e) {
            Log.w(TAG, "deleteFile " + name + ": " + e);
            return false;
        }
    }

    // ------------------------------------------------------------ «Поделиться»

    public static boolean share(Context ctx, String folder, String[] names) {
        ArrayList<Uri> uris = new ArrayList<>();
        try {
            if (folder.startsWith("content://")) {
                Uri tree = Uri.parse(folder);
                for (String n : names) {
                    Uri u = findChild(ctx.getContentResolver(), tree, n);
                    if (u != null) uris.add(u);
                }
            } else {
                String authority = ctx.getPackageName() + ".qtprovider";
                for (String n : names)
                    uris.add(FileProvider.getUriForFile(ctx, authority, new File(folder, n)));
            }
        } catch (Exception e) {
            Log.w(TAG, "share: " + e);
            return false;
        }
        if (uris.isEmpty()) return false;
        Intent send;
        if (uris.size() == 1) {
            send = new Intent(Intent.ACTION_SEND);
            send.putExtra(Intent.EXTRA_STREAM, uris.get(0));
        } else {
            send = new Intent(Intent.ACTION_SEND_MULTIPLE);
            send.putParcelableArrayListExtra(Intent.EXTRA_STREAM, uris);
        }
        send.setType("application/octet-stream");
        ClipData clip = ClipData.newRawUri("", uris.get(0));
        for (int i = 1; i < uris.size(); i++) clip.addItem(new ClipData.Item(uris.get(i)));
        send.setClipData(clip);
        send.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        Intent chooser = Intent.createChooser(send, "Поделиться результатами");
        chooser.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        if (!(ctx instanceof Activity)) chooser.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        ctx.startActivity(chooser);
        return true;
    }

    // ------------------------------------------------------------ служба расчёта (П3)

    public static void requestNotificationPermission(Context ctx) {
        if (Build.VERSION.SDK_INT < 33 || !(ctx instanceof Activity)) return;
        if (ctx.checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) == PackageManager.PERMISSION_GRANTED)
            return;
        final Activity a = (Activity) ctx;
        a.runOnUiThread(() -> a.requestPermissions(
                new String[] { Manifest.permission.POST_NOTIFICATIONS }, REQ_NOTIFICATIONS));
    }

    /** Язык интерфейса приложения ("ru"/"en") — надписи уведомления расчёта. */
    static volatile String sLang = "ru";

    public static void setLanguage(String code) {
        sLang = "en".equals(code) ? "en" : "ru";
    }

    static String tr(String ru, String en) {
        return "en".equals(sLang) ? en : ru;
    }

    public static void serviceStart(Context ctx, String title) {
        Intent it = new Intent(ctx, CalcService.class);
        it.setAction(CalcService.ACTION_START);
        it.putExtra(CalcService.EXTRA_TITLE, title);
        try {
            ctx.startForegroundService(it);
        } catch (Exception e) {
            // Запуск из фона запрещён (Android 12+) — расчёт идёт и без службы,
            // пока приложение на экране.
            Log.w(TAG, "startForegroundService: " + e);
        }
    }

    public static void serviceUpdate(Context ctx, String title, String text, int percent) {
        CalcService.update(ctx, title, text, percent);
    }

    public static void serviceStop(Context ctx) {
        ctx.stopService(new Intent(ctx, CalcService.class));
    }
}
