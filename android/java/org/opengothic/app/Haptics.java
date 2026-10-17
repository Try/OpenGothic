package org.opengothic.app;

import android.content.Context;
import android.os.Build;
import android.os.VibrationEffect;
import android.os.Vibrator;

/** Vibration, called from the native engine (common/utils/haptics.cpp). */
public final class Haptics {
    private static volatile Vibrator vibrator;

    private Haptics() {
    }

    static void init(Context context) {
        vibrator = (Vibrator) context.getSystemService(Context.VIBRATOR_SERVICE);
    }

    /** @param strength from 1 to 255 */
    public static void vibrate(int ms, int strength) {
        final Vibrator v = vibrator;
        if (v == null || !v.hasVibrator())
            return;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            final int amplitude = v.hasAmplitudeControl() ? Math.max(1, Math.min(255, strength))
                                                           : VibrationEffect.DEFAULT_AMPLITUDE;
            v.vibrate(VibrationEffect.createOneShot(ms, amplitude));
        } else {
            v.vibrate(ms);
        }
    }
}
