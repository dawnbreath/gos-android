#include "recomp_ctx.h"
#include <cstdio>
#include <cstring>
extern "C" void recomp_aapcs_call(recomp::Ctx* c, void* target, const void* stack, size_t stackBytes);
struct V3 { float x, y, z; };
struct Pair { long a, b; };
static V3 hfa(V3 a, float s, int i) { return { a.x * s + i, a.y * s, a.z * s }; }
static double mixed(int a, double b, long c, float d) { return a + b + c + d; }
static long many(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long s0, int s1, double f) { return a0+a1+a2+a3+a4+a5+a6+a7 + s0*100 + s1*1000 + (long)f; }
static Pair pair(long x) { return { x, x * 2 }; }
int main() {
  int fails = 0;
  recomp::Ctx c{};
  // hfa(V3{1,2,3}, 2.0f, 5): V3 in s0-s2, s in s3, i in w0
  c.v[0].f32[0]=1; c.v[1].f32[0]=2; c.v[2].f32[0]=3; c.v[3].f32[0]=2; c.x[0]=5;
  recomp_aapcs_call(&c, (void*)&hfa, nullptr, 0);
  if (c.v[0].f32[0]!=7 || c.v[1].f32[0]!=4 || c.v[2].f32[0]!=6) { puts("hfa FAIL"); fails++; } else puts("hfa ok");
  c = {}; c.x[0]=1; c.v[0].f64[0]=2.5; c.x[1]=3; c.v[1].f32[0]=0.5f;
  recomp_aapcs_call(&c, (void*)&mixed, nullptr, 0);
  if (c.v[0].f64[0]!=7.0) { puts("mixed FAIL"); fails++; } else puts("mixed ok");
  c = {}; for (int i=0;i<8;i++) c.x[i]=i+1; c.v[0].f64[0]=10000;
  uint64_t stk[2] = { 7, 9 };  // AAPCS64: 8-byte slots
  recomp_aapcs_call(&c, (void*)&many, stk, sizeof stk);
  if ((long)c.x[0] != 36 + 700 + 9000 + 10000) { printf("many FAIL %ld\n", (long)c.x[0]); fails++; } else puts("many ok");
  c = {}; c.x[0]=21; recomp_aapcs_call(&c, (void*)&pair, nullptr, 0);
  if (c.x[0]!=21 || c.x[1]!=42) { puts("pair FAIL"); fails++; } else puts("pair ok");
  return fails;
}
