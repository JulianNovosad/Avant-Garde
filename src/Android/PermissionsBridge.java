package com.avantgarde.blacknode;

import android.app.Activity;
import android.content.pm.PackageManager;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

public class PermissionsBridge {
    public static void requestPermissions(Activity activity, String[] perms, int reqCode){
        boolean need = false;
        for(String p: perms){ if(ContextCompat.checkSelfPermission(activity, p) != PackageManager.PERMISSION_GRANTED){ need=true; break; } }
        if(need){ ActivityCompat.requestPermissions(activity, perms, reqCode); }
    }
}
