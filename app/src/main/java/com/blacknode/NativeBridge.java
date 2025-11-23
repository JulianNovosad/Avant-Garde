package com.blacknode;

import android.app.Activity;
import android.content.pm.PackageManager;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

public class NativeBridge {
    static { System.loadLibrary("blacknode_native"); }

    private Activity activity;

    public NativeBridge(Activity a){ activity = a; }

    // Request runtime permissions if needed (CALLABLE from Java)
    public void requestNetworkPermissions(){
        String[] perms = new String[]{android.Manifest.permission.INTERNET};
        ActivityCompat.requestPermissions(activity, perms, 101);
    }

    // Simple bridge method callable from native side (optional)
    public native void nativeLog(String msg);
}
