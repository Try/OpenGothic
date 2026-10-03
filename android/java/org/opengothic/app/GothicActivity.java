package org.opengothic.app;

import android.app.NativeActivity;
import android.os.Bundle;

import java.io.File;

public class GothicActivity extends NativeActivity {
    static {
        System.loadLibrary("Gothic2Notr");
    }

    private static native void prepareStorage(String writablePath, String gamePath);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        File external = getExternalFilesDir(null);
        if (external == null) {
            throw new IllegalStateException("External application storage is unavailable");
        }
        File game = new File(external, "Gothic2");
        if (!game.isDirectory() && !game.mkdirs()) {
            throw new IllegalStateException("Unable to create the Gothic2 installation directory");
        }
        prepareStorage(getFilesDir().getAbsolutePath(), game.getAbsolutePath());
        super.onCreate(savedInstanceState);
    }
}
