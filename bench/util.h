#pragma once

#include <random>
#include <cstdint>
#include <map>
#include <ctime>
#include <cassert>
#include <iostream>
#include <string>
#include <algorithm>
#include <array>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

#if STR_SIZE < 5
#define STR_SIZE 15
#endif
#define NDEBUG                1
#ifdef _WIN32
	#ifndef NOMINMAX
	#define NOMINMAX
	#endif
    # define CONSOLE "CON"
    # define _CRT_SECURE_NO_WARNINGS 1
    # include <windows.h>
	
#else
    # define CONSOLE "/dev/tty"
    # include <unistd.h>
    # include <sys/resource.h>
    # include <sys/time.h>
#endif

#if __cplusplus > 201402L || _MSVC_LANG >= 201402L
   #define STR_VIEW  1
   #include <string_view>
#endif

#ifdef __has_include
    #if __has_include("wyhash.h")
    #include "wyhash.h"
    #endif
#endif

static int64_t getus()
{
#if 0
    auto tp = std::chrono::high_resolution_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(tp).count();

#elif WIN32_RSU
    FILETIME ptime[4] = {0, 0, 0, 0, 0, 0, 0, 0};
    GetThreadTimes(GetCurrentThread(), NULL, NULL, &ptime[2], &ptime[3]);
    return (ptime[2].dwLowDateTime + ptime[3].dwLowDateTime) / 10;
#elif WIN32_TICK
    return GetTickCount() * 1000;
#elif WIN32_HTIME || _WIN32
    LARGE_INTEGER freq = {0, 0};
    QueryPerformanceFrequency(&freq);

    LARGE_INTEGER nowus;
    QueryPerformanceCounter(&nowus);
    return (nowus.QuadPart * 1000000) / (freq.QuadPart);
#elif WIN32_STIME
    FILETIME    file_time;
    SYSTEMTIME  system_time;
    ULARGE_INTEGER ularge;

    GetSystemTime(&system_time);
    SystemTimeToFileTime(&system_time, &file_time);
    ularge.LowPart  = file_time.dwLowDateTime;
    ularge.HighPart = file_time.dwHighDateTime;
    return ularge.QuadPart / 10 + system_time.wMilliseconds / 1000;
#elif LINUX_RUS
    struct rusage rup;
    getrusage(RUSAGE_SELF, &rup);
    long sec  = rup.ru_utime.tv_sec  + rup.ru_stime.tv_sec;
    long usec = rup.ru_utime.tv_usec + rup.ru_stime.tv_usec;
    return sec * 1000000 + usec;
#elif LINUX_TICK || __APPLE__
    return clock();
#elif __linux__
    struct timespec ts = {0};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
#elif __unix__
    struct timeval start;
    gettimeofday(&start, NULL);
    return start.tv_sec * 1000000ull + start.tv_usec;
#else
    auto tp = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(tp).count();
#endif
}

static int ilog(int x, const int n = 2)
{
    int logn = 0;
    while (x / n) {
        logn ++;
        x /= n;
    }

    return logn;
}

static uint64_t randomseed() {
    std::random_device rd;
    return std::uniform_int_distribution<uint64_t>{}(rd);
}

#if __SIZEOF_INT128__
class Lehmer64 {
    __uint128_t g_lehmer64_state;

    uint64_t splitmix64_x; /* The state can be seeded with any value. */

    public:
    // call this one before calling splitmix64
    Lehmer64(uint64_t seed) { splitmix64_x = seed; }

    // returns random number, modifies splitmix64_x
    // compared with D. Lemire against
    // http://grepcode.com/file/repository.grepcode.com/java/root/jdk/openjdk/8-b132/java/util/SplittableRandom.java#SplittableRandom.0gamma
    inline uint64_t splitmix64(void) {
        uint64_t z = (splitmix64_x += UINT64_C(0x9E3779B97F4A7C15));
        z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
        z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
        return z ^ (z >> 31);
    }

    // returns the 32 least significant bits of a call to splitmix64
    // this is a simple function call followed by a cast
    inline uint32_t splitmix64_cast32(void) {
        return (uint32_t)splitmix64();
    }

    // same as splitmix64, but does not change the state, designed by D. Lemire
    inline uint64_t splitmix64_stateless(uint64_t index) {
        uint64_t z = (index + UINT64_C(0x9E3779B97F4A7C15));
        z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
        z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
        return z ^ (z >> 31);
    }

    inline void lehmer64_seed(uint64_t seed) {
        g_lehmer64_state = (((__uint128_t)splitmix64_stateless(seed)) << 64) +
            splitmix64_stateless(seed + 1);
    }

    inline uint64_t operator()() {
        g_lehmer64_state *= UINT64_C(0xda942042e4dd58b5);
        return g_lehmer64_state >> 64;
    }
};
#endif

class Orbit {
public:
    using result_type = uint64_t;

    static constexpr uint64_t(min)() {
        return 0;
    }
    static constexpr uint64_t(max)() {
        return UINT64_C(0xffffffffffffffff);
    }

    Orbit(uint64_t seed) noexcept
        : stateA(seed)
        , stateB(UINT64_C(0x9E6C63D0676A9A99)) {
        for (size_t i = 0; i < 10; ++i) {
            operator()();
        }
    }

    uint64_t operator()() noexcept {
        uint64_t s = (stateA += 0xC6BC279692B5C323u);
        uint64_t t = ((s == 0u) ? stateB : (stateB += 0x9E3779B97F4A7C15u));
        uint64_t z = (s ^ s >> 31) * ((t ^ t >> 22) | 1u);
        return z ^ z >> 26;
    }

private:
    static constexpr uint64_t rotl(uint64_t x, unsigned k) noexcept {
        return (x << k) | (x >> (64U - k));
    }

    uint64_t stateA;
    uint64_t stateB;
};

class RomuDuoJr {
public:
    using result_type = uint64_t;

    static constexpr uint64_t(min)() {
        return 0;
    }
    static constexpr uint64_t(max)() {
        return UINT64_C(0xffffffffffffffff);
    }

    RomuDuoJr(uint64_t seed) noexcept
        : mX(seed)
        , mY(UINT64_C(0x9E6C63D0676A9A99)) {
        for (size_t i = 0; i < 10; ++i) {
            operator()();
        }
    }

    uint64_t operator()() noexcept {
        uint64_t x = mX;

        mX = UINT64_C(15241094284759029579) * mY;
        mY = rotl(mY - x, 27);

        return x;
    }

private:
    static constexpr uint64_t rotl(uint64_t x, unsigned k) noexcept {
        return (x << k) | (x >> (64U - k));
    }

    uint64_t mX;
    uint64_t mY;
};

class Sfc4 {
public:
    using result_type = uint64_t;

    static constexpr uint64_t(min)() {
        return 0;
    }
    static constexpr uint64_t(max)() {
        return UINT64_C(0xffffffffffffffff);
    }

    Sfc4(uint64_t seed) noexcept
        : mA(seed)
        , mB(seed)
        , mC(seed)
        , mCounter(1) {
        for (size_t i = 0; i < 12; ++i) {
            operator()();
        }
    }

    uint64_t operator()() noexcept {
        uint64_t tmp = mA + mB + mCounter++;
        mA = mB ^ (mB >> 11U);
        mB = mC + (mC << 3U);
        mC = rotl(mC, 24U) + tmp;
        return tmp;
    }

private:
    static constexpr uint64_t rotl(uint64_t x, unsigned k) noexcept {
        return (x << k) | (x >> (64U - k));
    }

    uint64_t mA;
    uint64_t mB;
    uint64_t mC;
    uint64_t mCounter;
};


static inline uint64_t hashfib(uint64_t key)
{
#if __SIZEOF_INT128__
    __uint128_t r =  (__int128)key * UINT64_C(11400714819323198485);
    return (uint64_t)(r >> 64) + (uint64_t)r;
#elif _WIN64
    uint64_t high;
    return _umul128(key, UINT64_C(11400714819323198485), &high) + high;
#else
    uint64_t r = key * UINT64_C(0xca4bcaa75ec3f625);
    return (r >> 32) + r;
#endif
}

static inline uint64_t hashmix(uint64_t key)
{
    auto ror  = (key >> 32) | (key << 32);
    auto low  = key * 0xA24BAED4963EE407ull;
    auto high = ror * 0x9FB21C651E98DF25ull;
    auto mix  = low + high;
    return mix;// (mix >> 32) | (mix << 32);
}

static inline uint64_t rrxmrrxmsx_0(uint64_t v)
{
    /* Pelle Evensen's mixer, https://bit.ly/2HOfynt */
    v ^= (v << 39 | v >> 25) ^ (v << 14 | v >> 50);
    v *= UINT64_C(0xA24BAED4963EE407);
    v ^= (v << 40 | v >> 24) ^ (v << 15 | v >> 49);
    v *= UINT64_C(0x9FB21C651E98DF25);
    return v ^ v >> 28;
}

static inline uint64_t hash_mur3(uint64_t key)
{
    //MurmurHash3Mixer
    uint64_t h = key;
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccd;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53;
    h ^= h >> 33;
    return h;
}

template<typename T>
struct Int64Hasher
{
    static constexpr uint64_t KC = UINT64_C(11400714819323198485);
    inline std::size_t operator()(T key) const
    {
#if FIB_HASH == 1
        return key;
#elif FIB_HASH == 2
        return hashfib(key);
#elif FIB_HASH == 3
        return hash_mur3(key);
#elif FIB_HASH == 4
        return hashmix(key);
#elif FIB_HASH == 5
        return rrxmrrxmsx_0(key);
#elif FIB_HASH > 100
        return key % FIB_HASH; //bad hash
#elif FIB_HASH == 6
        return wyhash64(key, KC);
#else
        auto x = key;
        x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
        x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
        x = x ^ (x >> 31);
        return x;
#endif
    }
};

template<class RandomIt>
void shuffle(RandomIt first, RandomIt last)
{
    std::random_device rd;
    std::mt19937 g(rd());
    typedef typename std::iterator_traits<RandomIt>::difference_type diff_t;
    typedef std::uniform_int_distribution<diff_t> distr_t;
    typedef typename distr_t::param_type param_t;

    distr_t D;
    diff_t n = last - first;
    for (diff_t i = n-1; i > 0; --i) {
        using std::swap;
        swap(first[i], first[D(g, param_t(0, i))]);
    }
}

#if WYHASH_LITTLE_ENDIAN
struct WysHasher
{
    std::size_t operator()(const std::string& str) const
    {
        return wyhash(str.data(), str.size(), str.size());
    }
};

struct WyIntHasher
{
    uint64_t seed;
    WyIntHasher(uint64_t seed1 = randomseed())
    {
        seed = seed1;
    }

    std::size_t operator()(size_t v) const
    {
        return wyhash64(v, seed);
    }
};

struct WyRand
{
    uint64_t seed;
    WyRand(uint64_t seed1 = randomseed())
    {
        seed = seed1;
    }

    uint64_t operator()()
    {
        return wyrand(&seed);
    }
};
#endif

static void cpuidInfo(int regs[4], int id, int ext)
{
#if __x86_64__ || _M_X64 || _M_IX86 || __i386__
#if _MSC_VER >= 1600 //2010
    __cpuidex(regs, id, ext);
#elif __GNUC__
    __asm__ (
        "cpuid\n"
        : "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3])
        : "a"(id), "c"(ext)
    );
#elif ASM_X86
    __asm
    {
        mov eax, id
        mov ecx, ext
        cpuid
        mov edi, regs
        mov dword ptr [edi + 0], eax
        mov dword ptr [edi + 4], ebx
        mov dword ptr [edi + 8], ecx
        mov dword ptr [edi +12], edx
    }
#endif
#endif
}

static void printInfo(char* out)
{
    const char* sepator =
        "------------------------------------------------------------------------------------------------------------";

    puts(sepator);
    //    puts("Copyright (C) by 2019-2021 Huang Yuanbing bailuzhou at 163.com\n");

    char cbuff[512] = {0};
    char* info = cbuff;
#ifdef __clang__
    info += sprintf(info, "clang %s", __clang_version__); //vc/gcc/llvm
#if __llvm__
    info += sprintf(info, " on llvm/");
#endif
#endif

#if _MSC_VER
    info += sprintf(info, "ms vc++  %d", _MSC_VER);
#elif __GNUC__
    info += sprintf(info, "gcc %d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#elif __INTEL_COMPILER
    info += sprintf(info, "intel c++ %d", __INTEL_COMPILER);
#endif

#if __cplusplus > 199711L
    info += sprintf(info, " __cplusplus = %d", static_cast<int>(__cplusplus));
#endif

#if __x86_64__ || __amd64__ || _M_X64
    info += sprintf(info, " x86-64");
#elif __i386__ || _M_IX86
    info += sprintf(info, " x86");
#elif __arm64__ || __aarch64__
    info += sprintf(info, " arm64");
#elif __arm__
    info += sprintf(info, " arm");
#else
    info += sprintf(info, " unknow");
#endif

#if _WIN32
    info += sprintf(info, " OS = Win");
//    SetThreadAffinityMask(GetCurrentThread(), 0x1);
#elif __linux__
    info += sprintf(info, " OS = linux");
#elif __MAC__ || __APPLE__
    info += sprintf(info, " OS = MAC");
#elif __unix__
    info += sprintf(info, " OS = unix");
#else
    info += sprintf(info, " OS = unknow");
#endif

    info += sprintf(info, ", cpu = ");
    char vendor[0x40] = {0};
    int (*pTmp)[4] = (int(*)[4])vendor;
    cpuidInfo(*pTmp ++, 0x80000002, 0);
    cpuidInfo(*pTmp ++, 0x80000003, 0);
    cpuidInfo(*pTmp ++, 0x80000004, 0);

    info += sprintf(info, "%s", vendor);

    puts(cbuff);
    if (out)
        strcpy(out, cbuff);
    puts(sepator);
}

#include <chrono>
static const std::array<char, 62> ALPHANUMERIC_CHARS = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'
};

static std::uniform_int_distribution<std::size_t> rd_uniform(0, ALPHANUMERIC_CHARS.size() - 1);

static std::mt19937_64 generator(time(0));
static std::string get_random_alphanum_string(std::size_t size) {
    std::string str(size, '\0');

    const auto comm_head = size % 1 + 0;
    //test common head
    for(std::size_t i = 0; i < comm_head; i++) {
        str[i] = ALPHANUMERIC_CHARS[i];
    }
    for(std::size_t i = comm_head; i < size; i++) {
        str[i] = ALPHANUMERIC_CHARS[rd_uniform(generator)];
    }

    return str;
}

#if STR_VIEW
static char string_view_buf[1024 * 32] = {0};
static std::string_view get_random_alphanum_string_view(std::size_t size) {

    if (string_view_buf[0] == 0) {
        for(std::size_t i = 0; i < sizeof(string_view_buf); i++)
            string_view_buf[i] = ALPHANUMERIC_CHARS[rd_uniform(generator)];
    }

    auto start = generator() % (sizeof(string_view_buf) - size - 1);
    return {string_view_buf + start, size};
}
#endif
