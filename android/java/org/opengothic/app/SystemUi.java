package org.opengothic.app;

import android.app.Activity;
import android.view.View;

final class SystemUi {
    private SystemUi() {
    }

    /** Sticky immersive mode: the game draws behind the status and navigation bars. */
    static void hideBars(Activity activity) {
        final int flags = View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
        activity.getWindow().getDecorView().setSystemUiVisibility(flags);
    }
}
