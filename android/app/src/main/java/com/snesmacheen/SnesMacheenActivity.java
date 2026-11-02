package com.snesmacheen;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
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
                if (copied != null) selectedPath = copied.getAbsolutePath();
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
        if (savedUri == null || savedUri.isEmpty()) return;
        activity.importTreeAsync(Uri.parse(savedUri));
    }

    private void importTreeAsync(Uri treeUri) {
        new Thread(() -> {
            try {
                importRomFilesFromTree(treeUri);
                SDLActivity.onNativeDropFile(new File(getFilesDir(), "roms").getAbsolutePath());
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

    private boolean isSupportedRom(String name) {
        String lower = name.toLowerCase(Locale.ROOT);
        return lower.endsWith(".sfc") || lower.endsWith(".smc") ||
               lower.endsWith(".fig") || lower.endsWith(".swc");
    }

}
