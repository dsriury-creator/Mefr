package com.frmod;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.provider.Settings;
import android.widget.Toast;

public final class Loader {
    private static boolean started = false;

    private Loader() {}

    /** Chamado a partir do onCreate da Activity principal do jogo. */
    public static void start(final Activity a) {
        if (started) return;
        started = true;

        try {
            Native.setFlag(-1, 0);  // forca o carregamento da libfrmod.so
        } catch (Throwable t) {
            Toast.makeText(a, "FR Menu: falha ao carregar libfrmod.so", Toast.LENGTH_LONG).show();
        }

        final Handler h = new Handler(Looper.getMainLooper());

        if (Build.VERSION.SDK_INT >= 23 && !Settings.canDrawOverlays(a)) {
            Toast.makeText(a, "Permita \"Exibir sobre outros apps\" para abrir o FR Menu", Toast.LENGTH_LONG).show();
            try {
                Intent i = new Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION,
                        Uri.parse("package:" + a.getPackageName()));
                a.startActivity(i);
            } catch (Throwable t) {
                // sem tela de permissao: cai no modo dentro do jogo
            }
            final long t0 = SystemClock.uptimeMillis();
            h.postDelayed(new Runnable() {
                @Override
                public void run() {
                    if (Settings.canDrawOverlays(a)) {
                        ModMenu.show(a, true);
                    } else if (SystemClock.uptimeMillis() - t0 > 30000) {
                        // permissao nao concedida: mostra o menu dentro da propria janela do jogo
                        ModMenu.show(a, false);
                    } else {
                        h.postDelayed(this, 700);
                    }
                }
            }, 700);
        } else {
            ModMenu.show(a, true);
        }
    }
}
