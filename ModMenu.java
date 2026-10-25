package com.frmod;

import android.app.Activity;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.InsetDrawable;
import android.graphics.drawable.StateListDrawable;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.util.Base64;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.Switch;
import android.widget.TextView;

public final class ModMenu {
    private static final int RED = 0xFFE01B24;
    private static final int GREEN = 0xFF2ECC71;

    private final Activity act;
    private final boolean overlay;
    private WindowManager wm;
    private WindowManager.LayoutParams lp;
    private LinearLayout panel;
    private LinearLayout body;

    private Handler ui;
    private boolean closed = false;
    private TextView statusLabel;
    private TextView bounceInfo;
    private TextView debugInfo;
    private Button powerBtn;
    private int bounceLevel = 0;
    private int lastStatus = 0;

    private ModMenu(Activity a, boolean o) {
        act = a;
        overlay = o;
    }

    static void show(final Activity a, final boolean overlay) {
        a.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    new ModMenu(a, overlay).build();
                } catch (Throwable t) {
                    t.printStackTrace();
                }
            }
        });
    }

    private int dp(float v) {
        return (int) (v * act.getResources().getDisplayMetrics().density + 0.5f);
    }

    private static void flag(int id, int v) {
        try {
            Native.setFlag(id, v);
        } catch (Throwable ignored) {
        }
    }

    // ---------------------------------------------------------------- fundo com a foto
    /** Fundo reto: foto cortada para preencher + camada escura (pra ler o texto) + borda vermelha. */
    private static final class PhotoBg extends Drawable {
        private final Bitmap bmp;
        private final Paint photo = new Paint(Paint.FILTER_BITMAP_FLAG | Paint.ANTI_ALIAS_FLAG);
        private final Paint shade = new Paint();
        private final Paint border = new Paint();
        private final float bw;

        PhotoBg(Bitmap b, int borderPx) {
            bmp = b;
            bw = borderPx;
            shade.setColor(0x73000000);
            border.setColor(RED);
            border.setStyle(Paint.Style.STROKE);
            border.setStrokeWidth(borderPx);
        }

        @Override
        public void draw(Canvas c) {
            Rect r = getBounds();
            if (r.isEmpty()) return;
            if (bmp != null) {
                float w = bmp.getWidth();
                float h = bmp.getHeight();
                float scale = Math.max(r.width() / w, r.height() / h);
                float sw = r.width() / scale;
                float sh = r.height() / scale;
                int l = (int) ((w - sw) / 2f);
                int t = (int) ((h - sh) / 2f);
                c.drawBitmap(bmp, new Rect(l, t, (int) (l + sw), (int) (t + sh)), r, photo);
            } else {
                c.drawColor(0xFF2A0000);
            }
            c.drawRect(r, shade);
            float half = bw / 2f;
            c.drawRect(r.left + half, r.top + half, r.right - half, r.bottom - half, border);
        }

        @Override
        public void setAlpha(int alpha) {
        }

        @Override
        public void setColorFilter(ColorFilter cf) {
        }

        @Override
        public int getOpacity() {
            return PixelFormat.OPAQUE;
        }
    }

    private static Bitmap loadPhoto() {
        try {
            StringBuilder sb = new StringBuilder();
            for (String s : Photo.P) sb.append(s);
            byte[] b = Base64.decode(sb.toString(), Base64.DEFAULT);
            return BitmapFactory.decodeByteArray(b, 0, b.length);
        } catch (Throwable t) {
            return null;
        }
    }

    // ---------------------------------------------------------------- pecas visuais
    private TextView label(String text) {
        TextView t = new TextView(act);
        t.setText(text);
        t.setTextColor(Color.WHITE);
        t.setTextSize(13);
        t.setTypeface(Typeface.DEFAULT_BOLD);
        t.setPadding(dp(4), dp(10), dp(4), dp(2));
        return t;
    }

    private GradientDrawable pill(int fill, int stroke) {
        GradientDrawable g = new GradientDrawable();
        g.setShape(GradientDrawable.RECTANGLE);
        g.setColor(fill);
        g.setStroke(dp(1.5f), stroke);
        g.setCornerRadius(dp(12));
        g.setSize(dp(46), dp(24));
        return g;
    }

    private Drawable knob(int color) {
        GradientDrawable g = new GradientDrawable();
        g.setShape(GradientDrawable.OVAL);
        g.setColor(color);
        g.setSize(dp(18), dp(18));
        return new InsetDrawable(g, dp(3));
    }

    /** Botao de ligar/desligar redondo (trilho em pilula + bolinha). */
    private void addSwitch(LinearLayout parent, String text, final int id) {
        Switch s = new Switch(act);
        s.setText(text);
        s.setTextColor(Color.WHITE);
        s.setTextSize(14);
        s.setTypeface(Typeface.DEFAULT_BOLD);
        s.setPadding(dp(4), dp(7), dp(4), dp(7));

        StateListDrawable track = new StateListDrawable();
        track.addState(new int[]{android.R.attr.state_checked}, pill(RED, RED));
        track.addState(new int[]{}, pill(0xFF2A2A2A, 0xFF777777));
        StateListDrawable thumb = new StateListDrawable();
        thumb.addState(new int[]{android.R.attr.state_checked}, knob(Color.WHITE));
        thumb.addState(new int[]{}, knob(0xFFB5B5B5));
        s.setTrackDrawable(track);
        s.setThumbDrawable(thumb);

        s.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton b, boolean on) {
                flag(id, on ? 1 : 0);
            }
        });
        parent.addView(s, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
    }

    private void styleButton(Button b, int stroke) {
        GradientDrawable g = new GradientDrawable();
        g.setColor(0x99000000);
        g.setStroke(dp(2), stroke);
        g.setCornerRadius(dp(4));
        b.setBackground(g);
    }

    private Button button(String text, View.OnClickListener l) {
        Button b = new Button(act);
        b.setText(text);
        b.setTextColor(Color.WHITE);
        b.setTextSize(13);
        b.setTypeface(Typeface.DEFAULT_BOLD);
        b.setAllCaps(true);
        styleButton(b, RED);
        b.setOnClickListener(l);
        return b;
    }

    private void tint(SeekBar sb) {
        ColorStateList c = ColorStateList.valueOf(RED);
        sb.setProgressTintList(c);
        sb.setThumbTintList(c);
    }

    /** Slider de multiplicador: valor = minV + progresso * stepV; envia (valor * 100) como flag. */
    private void addSlider(LinearLayout list, final String name, final int id, int steps,
                           final float minV, final float stepV) {
        final TextView lab = label(name + ": x1.0");
        list.addView(lab);
        SeekBar sb = new SeekBar(act);
        tint(sb);
        sb.setMax(steps);
        sb.setProgress(Math.round((1f - minV) / stepV));
        sb.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar s, int p, boolean fromUser) {
                float v = minV + p * stepV;
                lab.setText(String.format("%s: x%.1f", name, v));
                flag(id, Math.round(v * 100f));
            }

            @Override
            public void onStartTrackingTouch(SeekBar s) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar s) {
            }
        });
        list.addView(sb);
    }

    // ---------------------------------------------------------------- status do mod
    private void refreshStatus() {
        if (closed) return;
        int st = 0;
        int base = -1;
        try {
            st = Native.getStatus();
            base = Native.getBounceBase();
        } catch (Throwable ignored) {
        }
        lastStatus = st;

        String t;
        String btn;
        int stroke = RED;
        switch (st) {
            case 1:
                t = "MOD: procurando o jogo...";
                btn = "AGUARDE...";
                break;
            case 2:
                t = "MOD: ATIVO";
                btn = "DESATIVAR MOD";
                stroke = GREEN;
                break;
            case 3:
                t = "MOD: erro (Update nao achado)";
                btn = "TENTAR DE NOVO";
                break;
            case 4:
                t = "MOD: erro (memoria)";
                btn = "TENTAR DE NOVO";
                break;
            case 5:
                t = "MOD: tempo esgotado";
                btn = "TENTAR DE NOVO";
                break;
            case 6:
                t = "MOD: DESLIGADO";
                btn = "ATIVAR MOD";
                break;
            default:
                t = "MOD: DESLIGADO (toque em ATIVAR)";
                btn = "ATIVAR MOD";
                break;
        }
        statusLabel.setText(t);
        statusLabel.setTextColor(st == 2 ? GREEN : Color.WHITE);
        powerBtn.setText(btn);
        styleButton(powerBtn, stroke);

        String info = "";
        if (bounceLevel > 0) {
            if (base >= 0) info = String.format("suspensao achada (base %.3f)", base / 1000f);
            else if (base == -2) info = "BOUNCE indisponivel nesta versao";
            else if (st == 2) info = "procurando a suspensao...";
            else info = "ative o mod para o BOUNCE funcionar";
        }
        bounceInfo.setText(info);

        String dbg = "";
        if (st == 2) {
            try {
                dbg = Native.getDebug();
            } catch (Throwable ignored) {
            }
        }
        debugInfo.setText(dbg);
    }

    private void build() {
        ui = new Handler(Looper.getMainLooper());

        panel = new LinearLayout(act);
        panel.setOrientation(LinearLayout.VERTICAL);
        panel.setBackground(new PhotoBg(loadPhoto(), dp(2)));
        panel.setPadding(dp(10), dp(8), dp(10), dp(10));

        // ---- titulo (arrastar = mover, tocar = minimizar/expandir)
        TextView title = new TextView(act);
        title.setText("FR MOD MENU");
        title.setTextColor(Color.WHITE);
        title.setTextSize(17);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        title.setLetterSpacing(0.12f);
        title.setGravity(Gravity.CENTER);
        title.setPadding(0, dp(4), 0, dp(6));
        title.setOnTouchListener(new View.OnTouchListener() {
            float offX, offY, startX, startY;
            boolean moved;

            @Override
            public boolean onTouch(View v, MotionEvent e) {
                switch (e.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        startX = e.getRawX();
                        startY = e.getRawY();
                        offX = curX() - startX;
                        offY = curY() - startY;
                        moved = false;
                        return true;
                    case MotionEvent.ACTION_MOVE:
                        if (Math.abs(e.getRawX() - startX) > 12 || Math.abs(e.getRawY() - startY) > 12) {
                            moved = true;
                        }
                        if (moved) moveTo((int) (e.getRawX() + offX), (int) (e.getRawY() + offY));
                        return true;
                    case MotionEvent.ACTION_UP:
                        if (!moved) toggleBody();
                        return true;
                    default:
                        return false;
                }
            }
        });
        panel.addView(title, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        View line = new View(act);
        line.setBackgroundColor(RED);
        panel.addView(line, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(2)));

        // ---- corpo (rolavel)
        body = new LinearLayout(act);
        body.setOrientation(LinearLayout.VERTICAL);

        ScrollView sv = new ScrollView(act);
        LinearLayout list = new LinearLayout(act);
        list.setOrientation(LinearLayout.VERTICAL);
        sv.addView(list);
        int maxH = (int) (act.getResources().getDisplayMetrics().heightPixels * 0.6f);
        body.addView(sv, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, maxH));

        // ---- liga/desliga o mod (e mostra o estado de verdade)
        statusLabel = label("MOD: DESLIGADO (toque em ATIVAR)");
        list.addView(statusLabel);
        powerBtn = button("ATIVAR MOD", new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (lastStatus == 2) {
                    flag(-2, 0);   // desativar
                } else if (lastStatus != 1) {
                    flag(-2, 1);   // ativar (ou tentar de novo)
                }
                ui.postDelayed(new Runnable() {
                    @Override
                    public void run() {
                        refreshStatus();
                    }
                }, 250);
            }
        });
        list.addView(powerBtn, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(44)));

        addSwitch(list, "THICK SMOKE", 0);
        addSwitch(list, "SMOKE DURATION++", 1);
        addSwitch(list, "CAR NO DAMAGE", 2);
        addSwitch(list, "TIRE NO DAMAGE", 3);

        // ---- velocidade / grip / direcao
        addSlider(list, "SPEED", 4, 40, 1.0f, 0.1f);   // x1.0 ate x5.0
        addSlider(list, "GRIP", 8, 25, 0.5f, 0.1f);    // x0.5 ate x3.0
        addSlider(list, "STEER", 9, 15, 0.5f, 0.1f);   // x0.5 ate x2.0
        addSwitch(list, "WALL: SEM PERDER VELOCIDADE", 10);

        // ---- bounce (quique da suspensao ao cair)
        final TextView bounceLabel = label("BOUNCE: OFF");
        list.addView(bounceLabel);
        SeekBar bb = new SeekBar(act);
        tint(bb);
        bb.setMax(100);
        bb.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar s, int p, boolean fromUser) {
                bounceLevel = p;
                bounceLabel.setText(p == 0 ? "BOUNCE: OFF" : "BOUNCE: " + p + "%");
                flag(6, p);
                refreshStatus();
            }

            @Override
            public void onStartTrackingTouch(SeekBar s) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar s) {
            }
        });
        list.addView(bb);
        bounceInfo = new TextView(act);
        bounceInfo.setTextColor(0xFFCCCCCC);
        bounceInfo.setTextSize(11);
        bounceInfo.setPadding(dp(4), 0, dp(4), dp(2));
        list.addView(bounceInfo);
        addSwitch(list, "BOUNCE INVERTIDO", 7);

        // ---- FPS alvo
        list.addView(label("TARGET FPS"));
        RadioGroup rg = new RadioGroup(act);
        rg.setOrientation(RadioGroup.HORIZONTAL);
        final int[] vals = {15, 30, 60, 120};
        for (int i = 0; i < vals.length; i++) {
            final int v = vals[i];
            RadioButton rb = new RadioButton(act);
            rb.setText(String.valueOf(v));
            rb.setTextColor(Color.WHITE);
            rb.setTextSize(13);
            rb.setButtonTintList(ColorStateList.valueOf(RED));
            rb.setId(View.generateViewId());
            rb.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
                @Override
                public void onCheckedChanged(CompoundButton b, boolean on) {
                    if (on) flag(5, v);
                }
            });
            rg.addView(rb, new RadioGroup.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        }
        list.addView(rg);

        // ---- diagnostico (numeros reais do jogo; me mande um print se algo nao funcionar)
        debugInfo = new TextView(act);
        debugInfo.setTextColor(0xFFBBBBBB);
        debugInfo.setTextSize(10);
        debugInfo.setPadding(dp(4), dp(8), dp(4), dp(2));
        list.addView(debugInfo);

        // ---- botoes
        LinearLayout row = new LinearLayout(act);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setPadding(0, dp(8), 0, 0);
        LinearLayout.LayoutParams bp = new LinearLayout.LayoutParams(0, dp(44), 1f);
        bp.setMargins(dp(3), 0, dp(3), 0);
        row.addView(button("MINIMIZE", new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                toggleBody();
            }
        }), bp);
        row.addView(button("FECHAR", new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                remove();
            }
        }), bp);
        body.addView(row);

        panel.addView(body);
        attach();

        // atualiza o status de tempos em tempos (ate o menu ser fechado)
        ui.post(new Runnable() {
            @Override
            public void run() {
                if (closed) return;
                refreshStatus();
                ui.postDelayed(this, 700);
            }
        });
    }

    // ---------------------------------------------------------------- janela
    private void attach() {
        if (overlay) {
            try {
                wm = (WindowManager) act.getApplicationContext().getSystemService(Context.WINDOW_SERVICE);
                int type = Build.VERSION.SDK_INT >= 26
                        ? WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
                        : WindowManager.LayoutParams.TYPE_PHONE;
                lp = new WindowManager.LayoutParams(
                        dp(260), ViewGroup.LayoutParams.WRAP_CONTENT, type,
                        WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                                | WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS,
                        PixelFormat.TRANSLUCENT);
                lp.gravity = Gravity.TOP | Gravity.START;
                lp.x = dp(10);
                lp.y = dp(30);
                wm.addView(panel, lp);
                return;
            } catch (Throwable t) {
                wm = null;  // sem permissao: cai para o modo dentro do jogo
            }
        }
        FrameLayout decor = (FrameLayout) act.getWindow().getDecorView();
        FrameLayout.LayoutParams f = new FrameLayout.LayoutParams(
                dp(260), ViewGroup.LayoutParams.WRAP_CONTENT);
        f.leftMargin = dp(10);
        f.topMargin = dp(30);
        decor.addView(panel, f);
    }

    private float curX() {
        return wm != null ? lp.x : panel.getX();
    }

    private float curY() {
        return wm != null ? lp.y : panel.getY();
    }

    private void moveTo(int x, int y) {
        if (wm != null) {
            lp.x = x;
            lp.y = y;
            wm.updateViewLayout(panel, lp);
        } else {
            panel.setX(x);
            panel.setY(y);
        }
    }

    private void toggleBody() {
        body.setVisibility(body.getVisibility() == View.VISIBLE ? View.GONE : View.VISIBLE);
    }

    private void remove() {
        closed = true;
        try {
            if (wm != null) {
                wm.removeView(panel);
            } else if (panel.getParent() instanceof ViewGroup) {
                ((ViewGroup) panel.getParent()).removeView(panel);
            }
        } catch (Throwable ignored) {
        }
    }

    // ---------------------------------------------------------------- foto do fundo (JPEG em base64)
    private static final class Photo {
        static final String[] P = {
            "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"
        };
    }
}
