package com.blacknode;

import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.Manifest;
import android.os.PowerManager;
import android.os.Build;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

public class BlacknodeBridge {
    static {
        try { System.loadLibrary("blacknode"); } catch (UnsatisfiedLinkError e) { /* ignore in desktop builds */ }
    }

    // Native hooks
    private static native void nativeInit();
    private static native void nativeOnPermissionResult(boolean granted);

    public static void requestPermissions(Activity act, int reqCode){
        String[] perms = new String[]{Manifest.permission.CAMERA, Manifest.permission.INTERNET, Manifest.permission.BODY_SENSORS};
        ActivityCompat.requestPermissions(act, perms, reqCode);
    }

    public static boolean hasPermissions(Context ctx){
        String[] perms = new String[]{Manifest.permission.CAMERA, Manifest.permission.INTERNET, Manifest.permission.BODY_SENSORS};
        for (String p: perms){
            if (ContextCompat.checkSelfPermission(ctx, p) != PackageManager.PERMISSION_GRANTED) return false;
        }
        return true;
    }

    private static PowerManager.WakeLock wakeLock = null;
    public static void acquireWakeLock(Context ctx){
        if (wakeLock!=null && wakeLock.isHeld()) return;
        PowerManager pm = (PowerManager)ctx.getSystemService(Context.POWER_SERVICE);
        if (pm==null) return;
        wakeLock = pm.newWakeLock(PowerManager.SCREEN_DIM_WAKE_LOCK | PowerManager.ACQUIRE_CAUSES_WAKEUP, "Blacknode:Wake");
        wakeLock.acquire();
        nativeInit();
    }

    public static void releaseWakeLock(){
        if (wakeLock!=null && wakeLock.isHeld()) wakeLock.release();
    }

    // Call this from Activity.onRequestPermissionsResult
    public static void onPermissionsResult(boolean granted){
        nativeOnPermissionResult(granted);
    }
}
