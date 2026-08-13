#include <pl/Mod.hpp>
#include <pl/memory/Hook.hpp>
#include <pl/memory/Signature.hpp>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <dlfcn.h>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct HookState { void* target{}; void* detour{}; };
using FaceFn = void (*)(void*, void*, const void*, const void*, const void*);

std::atomic_bool g_enabled{true};
std::mutex g_mutex;
std::vector<HookState*> g_hooks;

constexpr std::size_t BLOCK_TYPE_OFF = 0x68;
constexpr std::size_t NAME_INFO_OFF = 0x88;
constexpr std::size_t FULL_NAME_OFF = 0x40;
constexpr std::size_t HASHED_STRING_OFF = 0x8;

inline std::string_view blockName(const void* block) {
    if (!block) return {};
    const auto type = *reinterpret_cast<const uintptr_t*>(reinterpret_cast<uintptr_t>(block) + BLOCK_TYPE_OFF);
    if (!type) return {};
    const auto addr = type + NAME_INFO_OFF + FULL_NAME_OFF + HASHED_STRING_OFF;
    const auto* s = reinterpret_cast<const std::string*>(addr);
    if (!s || s->size() == 0 || s->size() > 256) return {};
    return {s->data(), s->size()};
}

inline std::string_view stripNs(std::string_view n) {
    constexpr std::string_view p = "minecraft:";
    if (n.starts_with(p)) n.remove_prefix(p.size());
    return n;
}

inline bool isOre(const void* block) {
    const auto n = stripNs(blockName(block));
    // All current vanilla ore families, including Nether ores and deepslate forms.
    return n == "coal_ore" || n == "iron_ore" || n == "copper_ore" ||
           n == "gold_ore" || n == "redstone_ore" || n == "lapis_ore" ||
           n == "diamond_ore" || n == "emerald_ore" ||
           n == "nether_gold_ore" || n == "quartz_ore" ||
           n == "ancient_debris" ||
           n == "deepslate_coal_ore" || n == "deepslate_iron_ore" ||
           n == "deepslate_copper_ore" || n == "deepslate_gold_ore" ||
           n == "deepslate_redstone_ore" || n == "deepslate_lapis_ore" ||
           n == "deepslate_diamond_ore" || n == "deepslate_emerald_ore";
}

// Signatures copied from the supplied BedrockTools reference, but resolved and hooked here directly.
constexpr const char* SIGS[] = {
#include "signatures.inc"
};

enum SigIndex { Down, Up, North, South, West, East, Count };
uintptr_t g_addr[Count]{};
FaceFn g_orig[Count]{};

void faceHook(int i, void* a0, void* a1, const void* block, const void* pos, const void* tex) {
    auto original = g_orig[i];
    if (!original) return;
    if (g_enabled.load(std::memory_order_relaxed) && !isOre(block)) return;
    original(a0, a1, block, pos, tex);
}
void down(void*a,void*b,const void*c,const void*d,const void*e){faceHook(Down,a,b,c,d,e);} 
void up(void*a,void*b,const void*c,const void*d,const void*e){faceHook(Up,a,b,c,d,e);} 
void north(void*a,void*b,const void*c,const void*d,const void*e){faceHook(North,a,b,c,d,e);} 
void south(void*a,void*b,const void*c,const void*d,const void*e){faceHook(South,a,b,c,d,e);} 
void west(void*a,void*b,const void*c,const void*d,const void*e){faceHook(West,a,b,c,d,e);} 
void east(void*a,void*b,const void*c,const void*d,const void*e){faceHook(East,a,b,c,d,e);} 
using Detour = void(*)(void*,void*,const void*,const void*,const void*);
constexpr Detour DETOURS[] = {down,up,north,south,west,east};

bool installOne(int i) {
    if (!g_addr[i]) return false;
    void* original = nullptr;
    if (pl::memory::hook(reinterpret_cast<void*>(g_addr[i]), reinterpret_cast<void*>(DETOURS[i]), &original) != 0) return false;
    g_orig[i] = reinterpret_cast<FaceFn>(original);
    g_hooks.push_back(new HookState{reinterpret_cast<void*>(g_addr[i]), reinterpret_cast<void*>(DETOURS[i])});
    return true;
}

void resolveAndInstall() {
    std::lock_guard lock(g_mutex);
    if (g_hooks.size() == Count) return;
    void* lib = dlopen("libminecraftpe.so", RTLD_NOW | RTLD_NOLOAD);
    if (!lib) return;
    std::vector<std::string> pats;
    for (auto s : SIGS) pats.emplace_back(s);
    auto found = pl::memory::resolveSignatures(pats, "libminecraftpe.so");
    dlclose(lib);
    for (int i=0;i<Count;i++) {
        auto it=found.find(pats[i]);
        if (it!=found.end()) g_addr[i]=it->second;
    }
    for (int i=0;i<Count;i++) if (!g_orig[i]) installOne(i);
}

class XRayMod {
public:
    static XRayMod& instance() { static XRayMod m; return m; }
    bool load(pl::mod::ModContext&) { return true; }
    bool enable(pl::mod::ModContext&) { g_enabled.store(true); resolveAndInstall(); return true; }
    bool disable(pl::mod::ModContext&) { g_enabled.store(false); return true; }
    bool unload(pl::mod::ModContext&) {
        g_enabled.store(false);
        std::lock_guard lock(g_mutex);
        for (auto* h : g_hooks) {
            if (h) { pl::memory::unhook(h->target, h->detour); delete h; }
        }
        g_hooks.clear();
        for (auto& o : g_orig) o = nullptr;
        return true;
    }
};
}

PL_REGISTER_MOD(XRayMod, XRayMod::instance())
