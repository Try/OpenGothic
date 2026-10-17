package org.opengothic.app;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * Imports a ZIP of the user's own Gothic II installation and starts the game.
 * The files are copied to the app-specific storage, no storage permissions are needed.
 */
public final class LauncherActivity extends Activity {
    private static final int REQUEST_IMPORT = 1;
    private static final int MAX_ENTRIES = 200000;
    private static final long MAX_SIZE = 20L * 1024 * 1024 * 1024;

    private TextView status;
    private Button start;
    private Button choose;
    private volatile boolean importing;
    private File root;

    @Override
    public void onCreate(Bundle state) {
        super.onCreate(state);
        File external = getExternalFilesDir(null);
        if (external == null) {
            finish();
            return;
        }
        root = new File(external, "Gothic2");
        SystemUi.hideBars(this);
        setContentView(buildUi());
        refresh();
    }

    private View buildUi() {
        LinearLayout page = new LinearLayout(this);
        page.setOrientation(LinearLayout.HORIZONTAL);
        page.setPadding(dp(32), dp(24), dp(32), dp(24));
        page.setBackground(new GradientDrawable(GradientDrawable.Orientation.TL_BR,
                new int[] {0xff071019, 0xff142e3e, 0xff080d14}));

        LinearLayout left = column(Gravity.CENTER_VERTICAL);
        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.addView(left);
        page.addView(scroll, new LinearLayout.LayoutParams(0, -1, 1.3f));

        TextView eyebrow = label("OPEN SOURCE  /  ANDROID", 12, 0xffa4afb4);
        eyebrow.setLetterSpacing(0.16f);
        left.addView(eyebrow);

        TextView title = label("OpenGothic", 42, 0xfff1e7cd);
        title.setTypeface(Typeface.create("serif", Typeface.NORMAL));
        left.addView(title);

        TextView subtitle = label("Return to Khorinis", 16, 0xffabbcc4);
        subtitle.setPadding(0, 0, 0, dp(20));
        left.addView(subtitle);

        start = button("Play", true);
        start.setOnClickListener(v -> startActivity(new Intent(this, GameActivity.class)));
        left.addView(start);

        choose = button("Import game files", false);
        choose.setOnClickListener(v -> pickArchive());
        left.addView(choose);

        Button help = button("How to start", false);
        help.setOnClickListener(v -> showHelp());
        left.addView(help);

        LinearLayout right = column(Gravity.CENTER_VERTICAL);
        right.setPadding(dp(40), 0, 0, 0);
        page.addView(right, new LinearLayout.LayoutParams(0, -1, 1.f));

        TextView edition = label("GOTHIC II", 25, 0xffd9cfb4);
        edition.setTypeface(Typeface.create("serif", Typeface.NORMAL));
        right.addView(edition);
        right.addView(label("NIGHT OF THE RAVEN", 14, 0xffa3b9c4));

        View line = new View(this);
        line.setBackgroundColor(0xff657581);
        LinearLayout.LayoutParams lineParams = new LinearLayout.LayoutParams(-1, dp(1));
        lineParams.setMargins(0, dp(22), 0, dp(22));
        right.addView(line, lineParams);

        status = label("", 16, 0xffc8d4da);
        right.addView(status);

        TextView note = label("Touch controls  •  Vulkan\nGraphics and controls can be adjusted in the game menu", 12, 0xff8fa5b0);
        note.setPadding(0, dp(24), 0, 0);
        right.addView(note);
        return page;
    }

    private LinearLayout column(int gravity) {
        LinearLayout ret = new LinearLayout(this);
        ret.setOrientation(LinearLayout.VERTICAL);
        ret.setGravity(gravity);
        return ret;
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    private TextView label(String text, int size, int color) {
        TextView view = new TextView(this);
        view.setText(text);
        view.setTextSize(size);
        view.setTextColor(color);
        return view;
    }

    private Button button(String text, boolean primary) {
        Button b = new Button(this);
        b.setText(text);
        b.setAllCaps(false);
        b.setTextSize(18);
        b.setGravity(Gravity.CENTER_VERTICAL | Gravity.LEFT);
        b.setPadding(dp(18), 0, dp(18), 0);
        b.setTextColor(new ColorStateList(
                new int[][] {new int[] {-android.R.attr.state_enabled}, new int[] {}},
                new int[] {0xff62727b, primary ? 0xff171c20 : 0xffe0e7e9}));

        GradientDrawable shape = new GradientDrawable();
        shape.setColor(primary ? 0xffd8c499 : 0x33172530);
        shape.setCornerRadius(dp(8));
        shape.setStroke(dp(1), primary ? 0xffe3d4b2 : 0xff425761);
        b.setBackground(new RippleDrawable(ColorStateList.valueOf(0x557f9caa), shape, null));

        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, dp(52));
        lp.setMargins(0, 0, dp(12), dp(9));
        b.setLayoutParams(lp);
        return b;
    }

    private void showHelp() {
        new AlertDialog.Builder(this)
                .setTitle("Getting started")
                .setMessage("1. Make a ZIP of your own Gothic II: Night of the Raven installation. "
                        + "It must contain the Data, _work and System folders.\n\n"
                        + "2. Choose Import game files. The files are copied once.\n\n"
                        + "3. Choose Play. Button size, camera sensitivity and graphics are in the settings of the in-game menu.\n\n"
                        + "The game files are not a part of the application.")
                .setPositiveButton("OK", null)
                .show();
    }

    private void pickArchive() {
        Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        pick.setType("*/*");
        pick.addCategory(Intent.CATEGORY_OPENABLE);
        startActivityForResult(pick, REQUEST_IMPORT);
    }

    private static boolean isInstallation(File dir) {
        return new File(dir, "Data").isDirectory()
                && new File(dir, "_work").isDirectory()
                && new File(dir, "System").isDirectory();
    }

    private void refresh() {
        boolean ready = isInstallation(root);
        start.setEnabled(ready && !importing);
        choose.setEnabled(!ready && !importing);
        choose.setVisibility(ready ? View.GONE : View.VISIBLE);
        status.setText(ready
                ? "Ready to play\nYour installation is available."
                : "Add your game\nImport a ZIP archive with the files of Gothic II: Night of the Raven.");
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request != REQUEST_IMPORT || result != RESULT_OK || data == null || data.getData() == null)
            return;

        importing = true;
        start.setEnabled(false);
        choose.setEnabled(false);
        status.setText("Importing... Keep the application open.");

        Uri uri = data.getData();
        new Thread(() -> importArchive(uri), "game-import").start();
    }

    /** Unpacks next to the final folder, which is created only after the whole archive is valid. */
    private void importArchive(Uri uri) {
        File staging = new File(root.getParentFile(), "import-staging");
        try {
            remove(staging);
            if (!staging.mkdirs())
                throw new IOException("Cannot create import directory");
            unzip(uri, staging);

            File source = findInstallation(staging);
            if (source == null)
                throw new IOException("Missing Data, _work or System. ZIP the complete game installation.");
            if (root.exists())
                throw new IOException("Gothic2 folder already exists; refusing to overwrite it");
            if (!source.renameTo(root))
                throw new IOException("Cannot finalize import");
            remove(staging);
            runOnUiThread(() -> {
                importing = false;
                refresh();
            });
        } catch (Exception error) {
            remove(staging);
            runOnUiThread(() -> {
                importing = false;
                refresh();
                status.setText("Import failed: " + error.getMessage());
            });
        }
    }

    private void unzip(Uri uri, File staging) throws IOException {
        String prefix = staging.getCanonicalPath() + File.separator;
        try (InputStream stream = getContentResolver().openInputStream(uri)) {
            if (stream == null)
                throw new IOException("Cannot open ZIP");
            try (ZipInputStream zip = new ZipInputStream(new BufferedInputStream(stream))) {
                byte[] buffer = new byte[131072];
                long total = 0;
                int count = 0;
                ZipEntry entry;
                while ((entry = zip.getNextEntry()) != null) {
                    if (++count > MAX_ENTRIES)
                        throw new IOException("Too many ZIP entries");
                    File out = new File(staging, entry.getName().replace('\\', '/'));
                    if (!out.getCanonicalPath().startsWith(prefix))
                        throw new IOException("Unsafe ZIP path");

                    if (entry.isDirectory()) {
                        makeDirs(out);
                        continue;
                    }
                    makeDirs(out.getParentFile());
                    try (OutputStream output = new BufferedOutputStream(new FileOutputStream(out))) {
                        int n;
                        while ((n = zip.read(buffer)) != -1) {
                            total += n;
                            if (total > MAX_SIZE)
                                throw new IOException("ZIP exceeds 20 GiB import limit");
                            output.write(buffer, 0, n);
                        }
                    }
                    if (count % 25 == 0) {
                        final long mb = total / (1024 * 1024);
                        runOnUiThread(() -> status.setText("Importing... " + mb + " MiB"));
                    }
                }
            }
        }
    }

    /** The installation is either at the root of the archive or in a single enclosing folder. */
    private static File findInstallation(File staging) throws IOException {
        if (isInstallation(staging))
            return staging;
        File found = null;
        File[] children = staging.listFiles();
        if (children == null)
            return null;
        for (File child : children) {
            if (!isInstallation(child))
                continue;
            if (found != null)
                throw new IOException("ZIP contains multiple installations");
            found = child;
        }
        return found;
    }

    private static void makeDirs(File dir) throws IOException {
        if (!dir.isDirectory() && !dir.mkdirs())
            throw new IOException("Cannot create folder");
    }

    private static void remove(File f) {
        File[] children = f.listFiles();
        if (children != null) {
            for (File c : children)
                remove(c);
        }
        if (f.exists())
            f.delete();
    }
}
