package work.wrkz.pluton_mobile_v2

import android.os.Bundle
import android.view.WindowManager
import io.flutter.embedding.android.FlutterActivity

/**
 * Sets FLAG_SECURE for the whole app.
 *
 * The wallet shows seed phrases and private keys on screen (setup backup and
 * Settings -> back up seed). Without this flag those screens can be captured
 * by screenshots, screen recorders, and the recents-screen thumbnail, which
 * persists after the app is backgrounded.
 */
class MainActivity : FlutterActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.setFlags(
            WindowManager.LayoutParams.FLAG_SECURE,
            WindowManager.LayoutParams.FLAG_SECURE
        )
    }
}
