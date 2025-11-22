package androidx.core.content;

import android.content.Context;
import android.content.pm.PackageManager;

public class ContextCompat {
    public static int checkSelfPermission(Context ctx, String perm){
        return PackageManager.PERMISSION_GRANTED;
    }
}
