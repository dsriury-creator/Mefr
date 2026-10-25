// FR Legends "MONEY PLATES" - lib nativa (arm64)
// Coloca 2 botoes DENTRO de Configuracoes > Outros:  [FPS: 60]  e  [SEM DANO: LIGADO]
// Tudo por NOME via API do IL2CPP (sem offsets). Nenhum menu Java.
//
// Como funciona:
//  - UISettingReplay.OnEnable (aba "Outros"): clona o botao "Lista negra" 2 vezes e troca os textos.
//  - FPSCounter.Update (roda sempre): olha se um dos botoes foi apertado (Selectable.IsPressed).
//  - DriftCarV4.Update: limpa as deformacoes do carro (sem dano) quando ligado.
//  - Cada trecho roda dentro de uma "rede de seguranca": se der SIGSEGV, o jogo nao fecha.
//  - Log em <pasta do app>/files/frmod_log.txt (me mande esse arquivo se algo nao funcionar).
#include <jni.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <atomic>
#include <cmath>
#include <csetjmp>
#include <csignal>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>

#ifdef __ANDROID__
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "FRMOD", __VA_ARGS__)
#else
#define LOGI(...) (printf(__VA_ARGS__), printf("\n"))
#endif

// ---------------------------------------------------------------- log em arquivo
static std::string g_dir;  // pasta files/ do app

static void initDir() {
    char pkg[200] = {0};
    FILE* f = fopen("/proc/self/cmdline", "r");
    if (f) { fread(pkg, 1, sizeof(pkg) - 1, f); fclose(f); }
    if (!pkg[0]) return;
    std::string a = std::string("/storage/emulated/0/Android/data/") + pkg + "/files";
    std::string b = std::string("/data/data/") + pkg + "/files";
    if (access(a.c_str(), W_OK) == 0) g_dir = a;
    else if (access(b.c_str(), W_OK) == 0) g_dir = b;
}

static void plog(const char* fmt, ...) {
    char buf[300];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    LOGI("%s", buf);
    if (g_dir.empty()) return;
    std::string p = g_dir + "/frmod_log.txt";
    FILE* f = fopen(p.c_str(), "a");
    if (!f) return;
    if (ftell(f) > 60000) { fclose(f); f = fopen(p.c_str(), "w"); if (!f) return; }
    fprintf(f, "%s\n", buf);
    fclose(f);
}

// ---------------------------------------------------------------- configuracao (salva em arquivo)
static std::atomic<int> cFpsIdx{0};    // 0 padrao | 1=15 | 2=30 | 3=60 | 4=120
static std::atomic<int> cNoDmg{0};     // carro sem dano
static const int kFps[5] = {0, 15, 30, 60, 120};

static void cfgSave() {
    if (g_dir.empty()) return;
    std::string p = g_dir + "/frmod_cfg.txt";
    FILE* f = fopen(p.c_str(), "w");
    if (!f) return;
    fprintf(f, "%d %d\n", cFpsIdx.load(), cNoDmg.load());
    fclose(f);
}

static void cfgLoad() {
    if (g_dir.empty()) return;
    std::string p = g_dir + "/frmod_cfg.txt";
    FILE* f = fopen(p.c_str(), "r");
    if (!f) return;
    int a = 0, b = 0;
    if (fscanf(f, "%d %d", &a, &b) == 2) {
        cFpsIdx = (a >= 0 && a <= 4) ? a : 0;
        cNoDmg = b ? 1 : 0;
    }
    fclose(f);
}

// ---------------------------------------------------------------- API do IL2CPP
static void* (*il2cpp_domain_get)();
static void** (*il2cpp_domain_get_assemblies)(void*, size_t*);
static void* (*il2cpp_assembly_get_image)(void*);
static const char* (*il2cpp_image_get_name)(void*);
static void* (*il2cpp_class_from_name)(void*, const char*, const char*);
static void* (*il2cpp_class_get_method_from_name)(void*, const char*, int);
static void* (*il2cpp_class_get_field_from_name)(void*, const char*);
static void* (*il2cpp_object_get_class)(void*);
static void (*il2cpp_field_get_value)(void*, void*, void*);
static void* (*il2cpp_runtime_invoke)(void*, void*, void**, void**);
static void* (*il2cpp_thread_attach)(void*);
static void* (*il2cpp_string_new)(const char*);
static void* (*il2cpp_object_new)(void*);
static void* (*il2cpp_class_get_methods)(void*, void**);
static const char* (*il2cpp_method_get_name)(void*);
static uint32_t (*il2cpp_method_get_param_count)(void*);
static void* (*il2cpp_method_get_param)(void*, uint32_t);
static char* (*il2cpp_type_get_name)(void*);
static void* (*il2cpp_class_get_type)(void*);
static void* (*il2cpp_type_get_object)(void*);

static bool resolveApi() {
    void* h = dlopen("libil2cpp.so", RTLD_NOW | RTLD_NOLOAD);
    if (!h) return false;
#define R(name)                                  \
    *(void**)&name = dlsym(h, #name);            \
    if (!name) { plog("falta simbolo %s", #name); return false; }
    R(il2cpp_domain_get)
    R(il2cpp_domain_get_assemblies)
    R(il2cpp_assembly_get_image)
    R(il2cpp_image_get_name)
    R(il2cpp_class_from_name)
    R(il2cpp_class_get_method_from_name)
    R(il2cpp_class_get_field_from_name)
    R(il2cpp_object_get_class)
    R(il2cpp_field_get_value)
    R(il2cpp_runtime_invoke)
    R(il2cpp_thread_attach)
    R(il2cpp_string_new)
    R(il2cpp_object_new)
    R(il2cpp_class_get_methods)
    R(il2cpp_method_get_name)
    R(il2cpp_method_get_param_count)
    R(il2cpp_method_get_param)
    R(il2cpp_type_get_name)
    R(il2cpp_class_get_type)
    R(il2cpp_type_get_object)
#undef R
    return true;
}

static void* findImage(const char* name) {
    void* dom = il2cpp_domain_get();
    if (!dom) return nullptr;
    size_t n = 0;
    void** asms = il2cpp_domain_get_assemblies(dom, &n);
    if (!asms) return nullptr;
    for (size_t i = 0; i < n; ++i) {
        void* img = il2cpp_assembly_get_image(asms[i]);
        const char* nm = img ? il2cpp_image_get_name(img) : nullptr;
        if (nm && !strcmp(nm, name)) return img;
    }
    return nullptr;
}

static void* findImageContains(const char* part) {
    void* dom = il2cpp_domain_get();
    if (!dom) return nullptr;
    size_t n = 0;
    void** asms = il2cpp_domain_get_assemblies(dom, &n);
    if (!asms) return nullptr;
    for (size_t i = 0; i < n; ++i) {
        void* img = il2cpp_assembly_get_image(asms[i]);
        const char* nm = img ? il2cpp_image_get_name(img) : nullptr;
        if (nm && strstr(nm, part)) return img;
    }
    return nullptr;
}

// ---------------------------------------------------------------- helpers de chamada
static std::map<std::pair<void*, std::string>, void*> methodCache;

static void* findMethod(void* klass, const char* name, int argc) {
    auto key = std::make_pair(klass, std::string(name) + "/" + std::to_string(argc));
    auto it = methodCache.find(key);
    if (it != methodCache.end()) return it->second;
    void* m = il2cpp_class_get_method_from_name(klass, name, argc);  // procura tambem nas classes pai
    methodCache[key] = m;
    return m;
}

// chama metodo de instancia pelo NOME (a classe vem do proprio objeto). nullptr se falhar.
static void* callO(void* obj, const char* name, int argc, void** args, bool* ok = nullptr) {
    if (ok) *ok = false;
    if (!obj) return nullptr;
    void* m = findMethod(il2cpp_object_get_class(obj), name, argc);
    if (!m) return nullptr;
    void* exc = nullptr;
    void* r = il2cpp_runtime_invoke(m, obj, args, &exc);
    if (exc) return nullptr;
    if (ok) *ok = true;
    return r;
}

static void* getRef(void* obj, const char* name) {
    if (!obj) return nullptr;
    void* f = il2cpp_class_get_field_from_name(il2cpp_object_get_class(obj), name);
    if (!f) return nullptr;
    void* out = nullptr;
    il2cpp_field_get_value(obj, f, &out);
    return out;
}

static void* typeObj(void* klass) { return il2cpp_type_get_object(il2cpp_class_get_type(klass)); }

static void* mkString(const char* s) { return il2cpp_string_new(s); }

// ---------------------------------------------------------------- Unity estatico (FPS)
static void* g_core = nullptr;  // UnityEngine.CoreModule.dll
static int origFps = -2;
static bool fpsChanged = false;

static int getStaticInt(const char* cls, const char* method) {
    void* k = il2cpp_class_from_name(g_core, "UnityEngine", cls);
    void* m = k ? il2cpp_class_get_method_from_name(k, method, 0) : nullptr;
    if (!m) return -1;
    void* exc = nullptr;
    char* r = (char*)il2cpp_runtime_invoke(m, nullptr, nullptr, &exc);
    if (exc || !r) return -1;
    return *(int*)(r + 16);
}

static void setStaticInt(const char* cls, const char* method, int val) {
    void* k = il2cpp_class_from_name(g_core, "UnityEngine", cls);
    void* m = k ? il2cpp_class_get_method_from_name(k, method, 1) : nullptr;
    if (!m) return;
    void* args[1] = {&val};
    void* exc = nullptr;
    il2cpp_runtime_invoke(m, nullptr, args, &exc);
}

static void applyFps() {
    if (!g_core) return;
    int want = kFps[cFpsIdx.load()];
    if (origFps == -2) origFps = getStaticInt("Application", "get_targetFrameRate");
    if (want > 0) {
        setStaticInt("QualitySettings", "set_vSyncCount", 0);
        setStaticInt("Application", "set_targetFrameRate", want);
        fpsChanged = true;
    } else if (fpsChanged && origFps != -2) {
        setStaticInt("Application", "set_targetFrameRate", origFps);
        fpsChanged = false;
    }
}

// ---------------------------------------------------------------- rede de seguranca contra crash
static sigjmp_buf g_jb;
static volatile sig_atomic_t g_guard = 0;
static pthread_t g_guardTid;
static struct sigaction g_oldSegv, g_oldBus;
static bool g_sigInstalled = false;

static void crashHandler(int sig, siginfo_t* si, void* ctx) {
    if (g_guard && pthread_equal(pthread_self(), g_guardTid)) {
        g_guard = 0;
        siglongjmp(g_jb, 1);
    }
    struct sigaction* old = (sig == SIGSEGV) ? &g_oldSegv : &g_oldBus;
    if (old->sa_flags & SA_SIGINFO) {
        if (old->sa_sigaction) { old->sa_sigaction(sig, si, ctx); return; }
    } else if (old->sa_handler != SIG_DFL && old->sa_handler != SIG_IGN) {
        old->sa_handler(sig);
        return;
    }
    signal(sig, SIG_DFL);
}

static void installCrashGuard() {
    if (g_sigInstalled) return;
    g_sigInstalled = true;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = crashHandler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, &g_oldSegv);
    sigaction(SIGBUS, &sa, &g_oldBus);
}

// roda fn(arg) protegida. Se travar, conta a falha e devolve false. (sem aninhar)
static bool runGuarded(void (*fn)(void*), void* arg, int* faults, const char* what) {
    if (*faults >= 3) return false;
    if (g_guard) { fn(arg); return true; }
    g_guardTid = pthread_self();
    if (sigsetjmp(g_jb, 1) == 0) {
        g_guard = 1;
        fn(arg);
        g_guard = 0;
        return true;
    }
    g_guard = 0;
    ++*faults;
    plog("FALHA (SIGSEGV) em %s (%d/3)", what, *faults);
    return false;
}

// ---------------------------------------------------------------- botoes dentro de Configuracoes > Outros
struct UiState {
    void* owner = nullptr;                 // UISettingReplay atual
    void* btn[2] = {nullptr, nullptr};     // componentes Button dos clones (MonoBehaviour: nao somem do GC)
    void* txt[2] = {nullptr, nullptr};     // Text / TMP_Text dos clones
    bool pressed[2] = {false, false};
    int cooldown = 0;
};
static UiState g_ui;
static void* g_clsButton = nullptr;
static void* g_clsText = nullptr;
static void* g_clsTmp = nullptr;
static void* g_clsObject = nullptr;
static void* g_mInstantiate = nullptr;

static const double kRow[2] = {2.8, 1.6};  // altura dos botoes novos acima do "Lista negra" (em alturas de botao)

static bool isAlive(void* comp) {
    bool ok = false;
    callO(comp, "get_isActiveAndEnabled", 0, nullptr, &ok);  // objeto destruido => excecao => ok=false
    return ok;
}

static void setLabel(int i, const char* s) {
    if (!g_ui.txt[i]) return;
    void* a[1] = {mkString(s)};
    callO(g_ui.txt[i], "set_text", 1, a);
}

static void refreshLabels() {
    char b[64];
    int idx = cFpsIdx.load();
    if (idx == 0) snprintf(b, sizeof(b), "FPS: PADRAO");
    else snprintf(b, sizeof(b), "FPS: %d", kFps[idx]);
    setLabel(0, b);
    setLabel(1, cNoDmg.load() ? "SEM DANO: LIGADO" : "SEM DANO: DESLIGADO");
}

static void* findInstantiate() {
    if (g_mInstantiate) return g_mInstantiate;
    void* it = nullptr;
    void* m;
    while ((m = il2cpp_class_get_methods(g_clsObject, &it)) != nullptr) {
        const char* n = il2cpp_method_get_name(m);
        if (!n || strcmp(n, "Instantiate") || il2cpp_method_get_param_count(m) != 2) continue;
        const char* a = il2cpp_type_get_name(il2cpp_method_get_param(m, 0));
        const char* b = il2cpp_type_get_name(il2cpp_method_get_param(m, 1));
        if (a && b && !strcmp(a, "UnityEngine.Object") && !strcmp(b, "UnityEngine.Transform")) {
            g_mInstantiate = m;
            break;
        }
    }
    return g_mInstantiate;
}

// cria os 2 botoes (clones do "Lista negra"). self = UISettingReplay
static void buildUiImpl(void* self) {
    if (g_ui.owner == self && g_ui.btn[0] && g_ui.btn[1] && isAlive(g_ui.btn[0]) && isAlive(g_ui.btn[1])) {
        refreshLabels();
        return;
    }
    void* bl = getRef(self, "blacklistButton");
    if (!bl) { plog("ui: campo blacklistButton nao achado"); return; }
    void* go = callO(bl, "get_gameObject", 0, nullptr);
    void* tr = callO(bl, "get_transform", 0, nullptr);
    void* parent = callO(tr, "get_parent", 0, nullptr);
    void* inst = findInstantiate();
    if (!go || !tr || !parent || !inst) {
        plog("ui: go=%p tr=%p parent=%p instantiate=%p", go, tr, parent, inst);
        return;
    }
    // posicao e altura do botao original
    char* rp = (char*)callO(tr, "get_anchoredPosition", 0, nullptr);
    char* rr = (char*)callO(tr, "get_rect", 0, nullptr);
    if (!rp || !rr) { plog("ui: sem anchoredPosition/rect"); return; }
    float px = *(float*)(rp + 16), py = *(float*)(rp + 20);
    float hh = *(float*)(rr + 16 + 12);  // Rect {x,y,width,height}

    UiState st;
    st.owner = self;
    for (int i = 0; i < 2; ++i) {
        void* args[2] = {go, parent};
        void* exc = nullptr;
        void* cgo = il2cpp_runtime_invoke(inst, nullptr, args, &exc);
        if (exc || !cgo) { plog("ui: Instantiate falhou (%d)", i); return; }

        void* ta[1] = {typeObj(g_clsButton)};
        void* cb = callO(cgo, "GetComponentInChildren", 1, ta);
        void* ct = nullptr;
        if (g_clsText) { void* a[1] = {typeObj(g_clsText)}; ct = callO(cgo, "GetComponentInChildren", 1, a); }
        if (!ct && g_clsTmp) { void* a[1] = {typeObj(g_clsTmp)}; ct = callO(cgo, "GetComponentInChildren", 1, a); }
        if (!cb) { plog("ui: Button do clone nao achado (%d)", i); return; }
        if (!ct) plog("ui: texto do clone nao achado (%d)", i);

        // tira o clique original (abrir "Lista negra") do clone: troca o onClick por um evento vazio
        void* ev = callO(cb, "get_onClick", 0, nullptr);
        if (ev) {
            void* ek = il2cpp_object_get_class(ev);
            void* ne = il2cpp_object_new(ek);
            callO(ne, ".ctor", 0, nullptr);
            void* a[1] = {ne};
            bool ok = false;
            callO(cb, "set_onClick", 1, a, &ok);
            if (!ok) plog("ui: set_onClick falhou (o clone ainda abre a Lista negra)");
        }

        // posiciona acima do original
        void* ctr = callO(cgo, "get_transform", 0, nullptr);
        float v[2] = {px, (float)(py + kRow[i] * hh)};
        void* pa[1] = {v};
        callO(ctr, "set_anchoredPosition", 1, pa);

        st.btn[i] = cb;
        st.txt[i] = ct;
    }
    g_ui = st;
    refreshLabels();
    plog("ui: botoes criados");
}

static void* g_arg = nullptr;
static void buildUiThunk(void* a) { buildUiImpl(a); }

static bool isPressed(void* btn) {
    bool ok = false;
    char* r = (char*)callO(btn, "IsPressed", 0, nullptr, &ok);
    return ok && r && *(r + 16);
}

static void onClick(int i) {
    if (i == 0) {
        cFpsIdx = (cFpsIdx.load() + 1) % 5;
        applyFps();
    } else {
        cNoDmg = cNoDmg.load() ? 0 : 1;
    }
    cfgSave();
    refreshLabels();
    plog("clique %d: fps=%d semdano=%d", i, kFps[cFpsIdx.load()], cNoDmg.load());
}

// ---------------------------------------------------------------- tick (todo frame, em qualquer tela)
static unsigned g_tickN = 0;
static int g_faultScan = 0;
static int g_faultTick = 0, g_faultUi = 0, g_faultCar = 0;

static void tickImpl(void*) {
    ++g_tickN;
    if (g_tickN == 5 || (g_tickN % 90) == 0) applyFps();  // o jogo pode voltar o FPS sozinho
    if (g_ui.cooldown > 0) --g_ui.cooldown;
    if (!g_ui.btn[0]) return;
    for (int i = 0; i < 2; ++i) {
        bool p = isPressed(g_ui.btn[i]);
        if (g_ui.pressed[i] && !p && g_ui.cooldown == 0) {  // soltou = clique
            g_ui.cooldown = 10;
            onClick(i);
        }
        g_ui.pressed[i] = p;
    }
}

// ---------------------------------------------------------------- esconde os textos promocionais (menu principal)
// Varre os elementos de interface ativos (Graphic = Text, TMP, Image...). Se o TEXTO tiver uma das frases
// abaixo, desativa o objeto. Tudo que aparece na tela e registrado no log (uma vez) para eu ajustar depois.
static void* g_clsGraphic = nullptr;
static void* g_mFindObjs = nullptr;
static std::set<void*> g_seen;
static int g_dumpLines = 0;
static const char* kBanWords[] = {"frlmods", "sign up to", "daily liveries", "community chat", "daily mods"};

static std::string strOf(void* s) {  // System.String -> ascii minusculo
    std::string o;
    if (!s) return o;
    int len = *(int*)((char*)s + 16);
    const unsigned short* ch = (const unsigned short*)((char*)s + 20);
    for (int i = 0; i < len && i < 160; ++i) o += (ch[i] < 128) ? (char)tolower(ch[i]) : '?';
    return o;
}

static void* findFindObjs() {
    if (g_mFindObjs) return g_mFindObjs;
    void* it = nullptr;
    void* m;
    while ((m = il2cpp_class_get_methods(g_clsObject, &it)) != nullptr) {
        const char* n = il2cpp_method_get_name(m);
        if (!n || strcmp(n, "FindObjectsOfType") || il2cpp_method_get_param_count(m) != 1) continue;
        const char* a = il2cpp_type_get_name(il2cpp_method_get_param(m, 0));
        if (a && !strcmp(a, "System.Type")) { g_mFindObjs = m; break; }
    }
    return g_mFindObjs;
}

static void scanImpl(void*) {
    if (!g_clsGraphic || !findFindObjs()) return;
    void* a[1] = {typeObj(g_clsGraphic)};
    void* exc = nullptr;
    char* arr = (char*)il2cpp_runtime_invoke(g_mFindObjs, nullptr, a, &exc);
    if (exc || !arr) return;
    long n = *(long*)(arr + 24);
    if (n <= 0 || n > 2000) return;
    void** el = (void**)(arr + 32);
    for (long i = 0; i < n; ++i) {
        void* g = el[i];
        if (!g) continue;
        bool ok = false;
        char* act = (char*)callO(g, "get_isActiveAndEnabled", 0, nullptr, &ok);
        if (!ok || !act || !*(act + 16)) continue;

        std::string text;
        bool hasText = false;
        void* ts = callO(g, "get_text", 0, nullptr, &hasText);
        if (hasText) text = strOf(ts);

        bool ban = false;
        for (const char* w : kBanWords) if (!text.empty() && text.find(w) != std::string::npos) ban = true;

        void* go = callO(g, "get_gameObject", 0, nullptr);
        if (ban && go) {
            bool f = false;
            void* sa[1] = {&f};
            callO(go, "SetActive", 1, sa);
            plog("escondido: texto='%s'", text.c_str());
            continue;
        }

        if (g_dumpLines < 160 && !g_seen.count(g)) {  // registra o que aparece na tela (1x por objeto)
            g_seen.insert(g);
            ++g_dumpLines;
            std::string nm = strOf(callO(go, "get_name", 0, nullptr));
            std::string sp;
            void* spr = callO(g, "get_sprite", 0, nullptr);
            if (spr) sp = strOf(callO(spr, "get_name", 0, nullptr));
            void* tr = callO(g, "get_transform", 0, nullptr);
            char* pos = (char*)callO(tr, "get_anchoredPosition", 0, nullptr);
            char* rc = (char*)callO(tr, "get_rect", 0, nullptr);
            char* kn = (char*)il2cpp_type_get_name(il2cpp_class_get_type(il2cpp_object_get_class(g)));
            plog("ui: %s go='%s' sprite='%s' texto='%s' pos=(%.0f,%.0f) tam=(%.0f,%.0f)", kn ? kn : "?", nm.c_str(),
                 sp.c_str(), text.c_str(), pos ? *(float*)(pos + 16) : 0.f, pos ? *(float*)(pos + 20) : 0.f,
                 rc ? *(float*)(rc + 24) : 0.f, rc ? *(float*)(rc + 28) : 0.f);
        }
    }
}

// ---------------------------------------------------------------- carro sem dano
static void* mIsAI = nullptr;
static bool mIsAIResolved = false;
static unsigned g_carN = 0;
static unsigned g_lastCarTick = 0;  // ultimo tick em que um carro rodou (= em corrida)

static bool isPlayerCar(void* car, void* cls) {
    if (!mIsAIResolved) {
        mIsAI = il2cpp_class_get_method_from_name(cls, "get_IsAIMovement", 0);
        mIsAIResolved = true;
    }
    if (mIsAI) {
        void* exc = nullptr;
        char* r = (char*)il2cpp_runtime_invoke(mIsAI, car, nullptr, &exc);
        if (!exc && r && *(r + 16)) return false;  // IA
    }
    void* f = il2cpp_class_get_field_from_name(cls, "<isGhost>k__BackingField");
    if (f) {
        unsigned char b = 0;
        il2cpp_field_get_value(car, f, &b);
        if (b) return false;  // fantasma
    }
    return true;
}

static void carImpl(void* car) {
    g_lastCarTick = g_tickN;
    if (!car || !cNoDmg.load()) return;
    if ((++g_carN % 6) != 0) return;
    void* cls = il2cpp_object_get_class(car);
    if (!isPlayerCar(car, cls)) return;
    void* br = getRef(car, "<bodyRoll>k__BackingField");
    void* st = getRef(br, "structure");
    callO(st, "ClearDeforms", 0, nullptr);
}

// ---------------------------------------------------------------- hooks (troca do ponteiro do metodo)
typedef void (*Fn2)(void*, void*);
static Fn2 oTick = nullptr, oOnEnable = nullptr, oCar = nullptr;

static void hkTick(void* self, void* mi) {
    oTick(self, mi);
    runGuarded(tickImpl, nullptr, &g_faultTick, "tick");
    // varre a interface a cada ~2,5 s, fora de corrida
    if ((g_tickN % 150) == 20 && g_tickN - g_lastCarTick > 120) runGuarded(scanImpl, nullptr, &g_faultScan, "varredura");
}

static void hkOnEnable(void* self, void* mi) {
    oOnEnable(self, mi);
    g_arg = self;
    runGuarded(buildUiThunk, self, &g_faultUi, "ui");
}

static void hkCar(void* self, void* mi) {
    oCar(self, mi);
    runGuarded(carImpl, self, &g_faultCar, "carro");
}

static bool hookMethod(void* klass, const char* name, void* hk, Fn2* orig) {
    void* mi = il2cpp_class_get_method_from_name(klass, name, 0);
    if (!mi) { plog("metodo %s nao achado", name); return false; }
    void** slot = (void**)mi;  // MethodInfo: [0]=methodPointer, [1]=virtualMethodPointer
    uintptr_t page = (uintptr_t)slot & ~(uintptr_t)4095;
    if (mprotect((void*)page, 8192, PROT_READ | PROT_WRITE) != 0) { plog("mprotect falhou"); return false; }
    void* o = slot[0];
    *orig = (Fn2)o;
    slot[0] = hk;
    if (slot[1] == o) slot[1] = hk;
    return true;
}

// ---------------------------------------------------------------- inicializacao (thread propria)
static bool g_api = false, g_attached = false;
static bool hTick = false, hUi = false, hCar = false;

static bool tryInit() {
    if (!g_api) {
        if (!resolveApi()) return false;
        g_api = true;
    }
    void* dom = il2cpp_domain_get();
    if (!dom) return false;
    if (!g_attached) {
        il2cpp_thread_attach(dom);
        g_attached = true;
    }
    void* img = findImage("Assembly-CSharp.dll");
    void* core = findImage("UnityEngine.CoreModule.dll");
    void* ui = findImage("UnityEngine.UI.dll");
    if (!img || !core || !ui) return false;
    g_core = core;
    g_clsObject = il2cpp_class_from_name(core, "UnityEngine", "Object");
    g_clsButton = il2cpp_class_from_name(ui, "UnityEngine.UI", "Button");
    g_clsText = il2cpp_class_from_name(ui, "UnityEngine.UI", "Text");
    void* tmp = findImageContains("TextMeshPro");
    g_clsTmp = tmp ? il2cpp_class_from_name(tmp, "TMPro", "TMP_Text") : nullptr;
    if (!g_clsObject || !g_clsButton) return false;
    g_clsGraphic = il2cpp_class_from_name(ui, "UnityEngine.UI", "Graphic");

    installCrashGuard();
    cfgLoad();
    void* kFpsC = il2cpp_class_from_name(img, "", "FPSCounter");
    void* kSet = il2cpp_class_from_name(img, "", "UISettingReplay");
    void* kCar = il2cpp_class_from_name(img, "", "DriftCarV4");
    if (!hTick && kFpsC) hTick = hookMethod(kFpsC, "Update", (void*)hkTick, &oTick);
    if (!hUi && kSet) hUi = hookMethod(kSet, "OnEnable", (void*)hkOnEnable, &oOnEnable);
    if (!hCar && kCar) hCar = hookMethod(kCar, "Update", (void*)hkCar, &oCar);
    plog("hooks: tick=%d ui=%d carro=%d (text=%p tmp=%p)", hTick, hUi, hCar, g_clsText, g_clsTmp);
    return hTick && hUi && hCar;
}

static void* initThread(void*) {
    initDir();
    plog("frmod plates: iniciando");
    for (int i = 0; i < 480; ++i) {  // ate ~4 minutos
        if (tryInit()) { plog("frmod plates: pronto"); return nullptr; }
        usleep(500 * 1000);
    }
    plog("frmod plates: desistiu (hooks tick=%d ui=%d carro=%d)", hTick, hUi, hCar);
    return nullptr;
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM*, void*) {
    pthread_t t;
    pthread_create(&t, nullptr, initThread, nullptr);  // sem menu: liga sozinho ao carregar a lib
    pthread_detach(t);
    return JNI_VERSION_1_6;
}
