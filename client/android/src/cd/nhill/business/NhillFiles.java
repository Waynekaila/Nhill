// Nhill — hands a file written by the application (PDF of a document, report, export)
// to another app: a viewer (ACTION_VIEW) or the share sheet (ACTION_SEND: e-mail,
// messaging). Android 7+ refuses file:// between apps: the file goes as a content://
// address of the FileProvider declared in AndroidManifest.xml, with a read grant for
// that file only.
package cd.nhill.business;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.util.Log;

import androidx.core.content.FileProvider;

import java.io.File;

public final class NhillFiles {
    private NhillFiles() {}

    public static boolean open(Context context, String path, String mime, boolean share, String title) {
        try {
            File file = new File(path);
            Uri uri = FileProvider.getUriForFile(context, context.getPackageName() + ".qtprovider", file);
            Intent intent;
            if (share) {
                intent = new Intent(Intent.ACTION_SEND);
                intent.setType(mime);
                intent.putExtra(Intent.EXTRA_STREAM, uri);
                intent.putExtra(Intent.EXTRA_SUBJECT, title);
            } else {
                intent = new Intent(Intent.ACTION_VIEW);
                intent.setDataAndType(uri, mime);
            }
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            Intent chooser = Intent.createChooser(intent, title);
            chooser.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_GRANT_READ_URI_PERMISSION);
            context.startActivity(chooser);
            return true;
        } catch (Exception e) {
            Log.e("Nhill", "cannot hand the file to another app: " + e);
            return false;
        }
    }
}
