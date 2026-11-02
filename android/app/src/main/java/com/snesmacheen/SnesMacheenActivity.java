package com.snesmacheen;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Build;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.provider.OpenableColumns;
import android.provider.DocumentsContract;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.BufferedInputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLEncoder;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.Set;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Locale;

/** Android SAF bridge: copies a selected ROM into app-private storage. */
public final class SnesMacheenActivity extends SDLActivity {
    private static final int REQUEST_PICK_ROM = 0x5341;
    private static final int REQUEST_PICK_ROM_FOLDER = 0x5342;
    private static final String PREFS_NAME = "snes_macheen_storage";
    private static final String PREF_ROM_TREE_URI = "rom_tree_uri";
    private static volatile SnesMacheenActivity instance;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        instance = this;
        super.onCreate(savedInstanceState);
        applyImmersiveMode();
    }

    @Override
    protected void onResume() {
        super.onResume();
        applyImmersiveMode();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) applyImmersiveMode();
    }

    /**
     * Keep the emulator edge-to-edge while it is open. On Android 11+ use
     * WindowInsetsController; older releases use the sticky immersive flags.
     * A swipe from an edge can still temporarily reveal system controls.
     */
    private void applyImmersiveMode() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                controller.setSystemBarsBehavior(
                    WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                controller.hide(WindowInsets.Type.statusBars() |
                    WindowInsets.Type.navigationBars());
            }
        } else {
            getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE |
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_FULLSCREEN);
        }
    }

    @Override
    protected void onDestroy() {
        if (instance == this) instance = null;
        super.onDestroy();
    }

    public static void openRomPicker() {
        final SnesMacheenActivity activity = instance;
        if (activity == null) return;
        activity.runOnUiThread(() -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            intent.putExtra(Intent.EXTRA_MIME_TYPES, new String[] { "application/octet-stream" });
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            try {
                activity.startActivityForResult(intent, REQUEST_PICK_ROM);
            } catch (ActivityNotFoundException exception) {
                SDLActivity.onNativeDropFile("");
            }
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQUEST_PICK_ROM_FOLDER) {
            if (resultCode != Activity.RESULT_OK || data == null || data.getData() == null) return;
            Uri treeUri = data.getData();
            int grantedFlags = data.getFlags() &
                (Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            try {
                getContentResolver().takePersistableUriPermission(treeUri, grantedFlags);
                getSharedPreferences(PREFS_NAME, MODE_PRIVATE).edit()
                    .putString(PREF_ROM_TREE_URI, treeUri.toString()).apply();
                importTreeAsync(treeUri);
            } catch (SecurityException exception) {
                SDLActivity.onNativeDropFile("");
            }
            return;
        }
        if (requestCode != REQUEST_PICK_ROM) return;
        if (resultCode != Activity.RESULT_OK || data == null || data.getData() == null) {
            SDLActivity.onNativeDropFile("");
            return;
        }
        final Uri uri = data.getData();
        new Thread(() -> {
            String selectedPath = "";
            try {
                File copied = copyRomToPrivateStorage(uri);
                if (copied != null) {
                    selectedPath = copied.getAbsolutePath();
                    downloadMissingCoversSync(copied.getParentFile());
                }
            } catch (IOException ignored) {
                selectedPath = "";
            }
            SDLActivity.onNativeDropFile(selectedPath);
        }, "SnesMacheenRomImport").start();
    }

    public static void openRomFolderPicker() {
        final SnesMacheenActivity activity = instance;
        if (activity == null) return;
        activity.runOnUiThread(() -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION |
                Intent.FLAG_GRANT_WRITE_URI_PERMISSION |
                Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION |
                Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);
            try {
                activity.startActivityForResult(intent, REQUEST_PICK_ROM_FOLDER);
            } catch (ActivityNotFoundException exception) {
                SDLActivity.onNativeDropFile("");
            }
        });
    }

    public static void importPersistedRomFolder() {
        final SnesMacheenActivity activity = instance;
        if (activity == null) return;
        SharedPreferences preferences = activity.getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        String savedUri = preferences.getString(PREF_ROM_TREE_URI, null);
        if (savedUri == null || savedUri.isEmpty()) {
            activity.downloadMissingCovers(new File(activity.getFilesDir(), "roms").getAbsolutePath());
            return;
        }
        activity.importTreeAsync(Uri.parse(savedUri));
    }

    private void importTreeAsync(Uri treeUri) {
        new Thread(() -> {
            try {
                File romDirectory = new File(getFilesDir(), "roms");
                importRomFilesFromTree(treeUri);
                downloadMissingCoversSync(romDirectory);
                SDLActivity.onNativeDropFile(romDirectory.getAbsolutePath());
            } catch (Exception exception) {
                SDLActivity.onNativeDropFile("");
            }
        }, "SnesMacheenFolderImport").start();
    }

    private void importRomFilesFromTree(Uri treeUri) throws IOException {
        String treeDocumentId = DocumentsContract.getTreeDocumentId(treeUri);
        Uri childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, treeDocumentId);
        String[] columns = {
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_MIME_TYPE
        };
        try (Cursor cursor = getContentResolver().query(childrenUri, columns, null, null, null)) {
            if (cursor == null) throw new IOException("Unable to list the selected ROM folder");
            int idColumn = cursor.getColumnIndex(DocumentsContract.Document.COLUMN_DOCUMENT_ID);
            int nameColumn = cursor.getColumnIndex(DocumentsContract.Document.COLUMN_DISPLAY_NAME);
            int mimeColumn = cursor.getColumnIndex(DocumentsContract.Document.COLUMN_MIME_TYPE);
            if (idColumn < 0 || nameColumn < 0 || mimeColumn < 0) {
                throw new IOException("Selected folder does not provide document details");
            }
            while (cursor.moveToNext()) {
                String documentId = cursor.getString(idColumn);
                String displayName = cursor.getString(nameColumn);
                String mimeType = cursor.getString(mimeColumn);
                if (DocumentsContract.Document.MIME_TYPE_DIR.equals(mimeType) ||
                    displayName == null || !isSupportedRom(displayName)) continue;
                Uri documentUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, documentId);
                copyRomToPrivateStorage(documentUri, displayName);
            }
        }
    }

    private File copyRomToPrivateStorage(Uri uri) throws IOException {
        return copyRomToPrivateStorage(uri, queryDisplayName(uri));
    }

    private File copyRomToPrivateStorage(Uri uri, String displayName) throws IOException {
        if (!isSupportedRom(displayName)) return null;
        File directory = new File(getFilesDir(), "roms");
        if (!directory.exists() && !directory.mkdirs()) {
            throw new IOException("Unable to create private ROM directory");
        }
        String safeName = displayName.replaceAll("[^A-Za-z0-9._ -]", "_");
        File destination = new File(directory, safeName);
        try (InputStream input = getContentResolver().openInputStream(uri);
             OutputStream output = new FileOutputStream(destination, false)) {
            if (input == null) throw new IOException("Selected document cannot be opened");
            byte[] buffer = new byte[64 * 1024];
            int count;
            while ((count = input.read(buffer)) != -1) output.write(buffer, 0, count);
            output.flush();
        }
        return destination;
    }

    private String queryDisplayName(Uri uri) {
        String name = null;
        try (Cursor cursor = getContentResolver().query(
                uri, new String[] { OpenableColumns.DISPLAY_NAME }, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                int column = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (column >= 0) name = cursor.getString(column);
            }
        }
        if (name == null || name.trim().isEmpty()) name = uri.getLastPathSegment();
        return name == null ? "selected-rom" : name;
    }

    /** Download missing SNES box art to the private ROM folder. */
    public static void downloadMissingCovers(String directoryPath) {
        final SnesMacheenActivity activity = instance;
        if (activity == null || directoryPath == null) return;
        new Thread(() -> {
            activity.downloadMissingCoversSync(new File(directoryPath));
            SDLActivity.onNativeDropFile(directoryPath);
        }, "SnesMacheenCoverDownload").start();
    }

    private void downloadMissingCoversSync(File romDirectory) {
        if (romDirectory == null || !romDirectory.isDirectory()) return;
        File coverDirectory = new File(romDirectory, "covers");
        if (!coverDirectory.exists() && !coverDirectory.mkdirs()) return;
        File[] roms = romDirectory.listFiles((dir, name) -> isSupportedRom(name));
        if (roms == null) return;
        int attempted = 0;
        for (File rom : roms) {
            if (attempted++ >= 200) break;
            String filename = rom.getName();
            String stem = filename.substring(0, filename.lastIndexOf('.'));
            File destination = new File(coverDirectory, stem + ".bmp");
            if (destination.isFile() && destination.length() > 54) continue;
            for (String candidate : coverNameCandidates(stem)) {
                HttpURLConnection connection = null;
                try {
                    String encoded = URLEncoder.encode(candidate, StandardCharsets.UTF_8.name())
                        .replace("+", "%20");
                    String base = "https://thumbnails.libretro.com/Nintendo%20-%20Super%20Nintendo%20Entertainment%20System/Named_Boxarts/";
                    connection = (HttpURLConnection) new URL(base + encoded + ".png").openConnection();
                    connection.setConnectTimeout(3500);
                    connection.setReadTimeout(6000);
                    connection.setRequestProperty("User-Agent", "Snes-Macheen/1.0 (Android)");
                    connection.setInstanceFollowRedirects(true);
                    if (connection.getResponseCode() != HttpURLConnection.HTTP_OK) continue;
                    try (InputStream input = new BufferedInputStream(connection.getInputStream())) {
                        Bitmap bitmap = BitmapFactory.decodeStream(input);
                        if (bitmap != null) {
                            writeBitmapAsBmp(bitmap, destination);
                            bitmap.recycle();
                            if (destination.isFile() && destination.length() > 54) break;
                        }
                    }
                } catch (Exception ignored) {
                    // Missing regional variants and offline access are non-fatal.
                } finally {
                    if (connection != null) connection.disconnect();
                }
            }
        }
    }

    private ArrayList<String> coverNameCandidates(String stem) {
        Set<String> names = new LinkedHashSet<>();
        names.add(stem);
        names.add(toTitleCase(stem));
        String upper = stem.toUpperCase(Locale.ROOT);
        if (upper.endsWith(" USA")) {
            String base = stem.substring(0, stem.length() - 4).trim();
            names.add(base + " (USA)");
            names.add(toTitleCase(base) + " (USA)");
            names.add(toTitleCase(base));
        } else if (upper.endsWith(" EUR") || upper.endsWith(" EUROPE")) {
            String base = stem.replaceFirst("(?i)\\s+(EUR|EUROPE)$", "").trim();
            names.add(toTitleCase(base) + " (Europe)");
        } else if (upper.endsWith(" JAP") || upper.endsWith(" JAPAN")) {
            String base = stem.replaceFirst("(?i)\\s+(JAP|JAPAN)$", "").trim();
            names.add(toTitleCase(base) + " (Japan)");
        }
        return new ArrayList<>(names);
    }

    private String toTitleCase(String value) {
        String lower = value.toLowerCase(Locale.ROOT);
        StringBuilder result = new StringBuilder(lower.length());
        boolean capitalize = true;
        for (int i = 0; i < lower.length(); i++) {
            char ch = lower.charAt(i);
            result.append(capitalize ? Character.toUpperCase(ch) : ch);
            capitalize = Character.isWhitespace(ch) || ch == '-' || ch == '(';
        }
        return result.toString();
    }

    private void writeBitmapAsBmp(Bitmap source, File destination) throws IOException {
        int width = source.getWidth();
        int height = source.getHeight();
        if (width > 320 || height > 480) {
            float scale = Math.min(320f / width, 480f / height);
            Bitmap scaled = Bitmap.createScaledBitmap(source,
                Math.max(1, Math.round(width * scale)),
                Math.max(1, Math.round(height * scale)), true);
            if (scaled != source) {
                writeBitmapAsBmp(scaled, destination);
                scaled.recycle();
                return;
            }
        }
        int rowBytes = ((width * 3 + 3) / 4) * 4;
        int imageBytes = rowBytes * height;
        try (OutputStream output = new FileOutputStream(destination, false)) {
            output.write('B'); output.write('M');
            writeIntLE(output, 54 + imageBytes);
            writeIntLE(output, 0); writeIntLE(output, 54);
            writeIntLE(output, 40); writeIntLE(output, width); writeIntLE(output, height);
            writeShortLE(output, 1); writeShortLE(output, 24);
            writeIntLE(output, 0); writeIntLE(output, imageBytes);
            writeIntLE(output, 2835); writeIntLE(output, 2835);
            writeIntLE(output, 0); writeIntLE(output, 0);
            int[] pixels = new int[width];
            byte[] row = new byte[rowBytes];
            for (int y = height - 1; y >= 0; y--) {
                source.getPixels(pixels, 0, width, 0, y, width, 1);
                int offset = 0;
                for (int pixel : pixels) {
                    row[offset++] = (byte) (pixel & 0xff);
                    row[offset++] = (byte) ((pixel >> 8) & 0xff);
                    row[offset++] = (byte) ((pixel >> 16) & 0xff);
                }
                while (offset < row.length) row[offset++] = 0;
                output.write(row);
            }
            output.flush();
        }
    }

    private void writeIntLE(OutputStream output, int value) throws IOException {
        output.write(value & 0xff); output.write((value >> 8) & 0xff);
        output.write((value >> 16) & 0xff); output.write((value >> 24) & 0xff);
    }

    private void writeShortLE(OutputStream output, int value) throws IOException {
        output.write(value & 0xff); output.write((value >> 8) & 0xff);
    }

    private boolean isSupportedRom(String name) {
        String lower = name.toLowerCase(Locale.ROOT);
        return lower.endsWith(".sfc") || lower.endsWith(".smc") ||
               lower.endsWith(".fig") || lower.endsWith(".swc");
    }

}
