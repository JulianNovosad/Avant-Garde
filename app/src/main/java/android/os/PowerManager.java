package android.os;

public class PowerManager {
    public static final int SCREEN_DIM_WAKE_LOCK = 1;
    public static final int ACQUIRE_CAUSES_WAKEUP = 2;

    public static class WakeLock {
        private boolean held = false;
        public void acquire(){ held = true; }
        public void release(){ held = false; }
        public boolean isHeld(){ return held; }
    }

    public WakeLock newWakeLock(int flags, String tag){ return new WakeLock(); }
}
