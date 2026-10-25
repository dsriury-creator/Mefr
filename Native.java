package com.frmod;

public final class Native {
    static {
        System.loadLibrary("frmod");
    }

    private Native() {}

    /**
     * 0 thick smoke | 1 smoke duration | 2 car no damage | 3 tire no damage | 4 speed % | 5 fps
     * 6 bounce (0..100) | 7 bounce invertido | 8 grip % | 9 steer % | 10 wall sem perda de velocidade
     * -2 mod: 1 = ligar / 0 = desligar
     */
    public static native void setFlag(int id, int value);

    /** 0 desligado | 1 procurando | 2 ativo | 3 Update nao achado | 4 erro de memoria | 5 tempo esgotado | 6 pausado */
    public static native int getStatus();

    /** texto de diagnostico (valores reais do jogo) */
    public static native String getDebug();

    /** valor original do bounceRatio da suspensao x1000 | -1 suspensao nao achada | -2 API indisponivel */
    public static native int getBounceBase();
}
