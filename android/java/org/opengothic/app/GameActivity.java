package org.opengothic.app;

import android.app.NativeActivity;
import android.os.Build;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.WindowManager;

/**
 * The game itself. Controls are drawn and handled by the native engine, there is no Java view under the surface.
 */
public final class GameActivity extends NativeActivity {
    static {
        // NativeActivity does not call JNI_OnLoad, the engine needs it to reach Haptics
        System.loadLibrary("Gothic2Notr");
    }

    @Override
    public void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            WindowManager.LayoutParams lp = getWindow().getAttributes();
            lp.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            getWindow().setAttributes(lp);
        }
        Haptics.init(this);
        SystemUi.hideBars(this);
    }

    @Override
    public void onWindowFocusChanged(boolean focused) {
        super.onWindowFocusChanged(focused);
        if (focused)
            SystemUi.hideBars(this);
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        // The native input queue receives touches first. What it did not handle must not turn into Java clicks.
        return true;
    }
}
