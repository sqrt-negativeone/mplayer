/* date = December 18th 2024 1:07 am */

#ifndef MPLAYER_RANDOM_H
#define MPLAYER_RANDOM_H

typedef struct Random_Generator Random_Generator;
struct Random_Generator
{
  u32 r;
  u32 a;
  u32 c;
  u32 m;
};

internal Random_Generator rng_make(u32 seed, u32 a, u32 c, u32 m);
internal Random_Generator rng_make_linear(u32 seed);
internal u32      rng_next(Random_Generator *rng);
internal u32      rng_next_minmax(Random_Generator *rng, u32 min, u32 max);
internal f32      rng_next_f01(Random_Generator *rng);
internal f32      rng_next_fn1(Random_Generator *rng);


internal Random_Generator
rng_make(u32 seed, u32 a, u32 c, u32 m)
{
  Random_Generator result =
  {
    .r = seed,
    .a = a,
    .c = c,
    .m = m
  };
	
  rng_next(&result);
  return result;
}

internal Random_Generator
rng_make_linear(u32 seed)
{
  // NOTE(fakhri): linear congruential generator
  // see https://en.wikipedia.org/wiki/Linear_congruential_generator
  // paramters for the glibc used by GCC
  u32 a = 1103515245;
  u32 c = 12345;
  u32 m = 1u << 31;
  Random_Generator result = rng_make(seed, a, c, m);
  return result;
}

internal u32
rng_next(Random_Generator *rng)
{
  rng->r = (rng->a * rng->r + rng->c) % rng->m; 
  u32 result = rng->r;
  return result;
}

internal u32
rng_next_minmax(Random_Generator *rng, u32 min, u32 max)
{
  u32 diff = max - min;
  assert(diff);
  u32 result = 0;
  if (diff)
  {
    u32 r = rng_next(rng);
    result = min + r / (rng->m / (max - min) + 1);
  }
  assert(result < max);
  assert(result >= min);
  return result;
}

internal f32
rng_next_f01(Random_Generator *rng)
{
  u32 r = rng_next(rng);
  f32 result = (f32)r / (f32)rng->m;
  return result;
}

internal f32
rng_next_fn1(Random_Generator *rng)
{
  f32 result = -1.0f + 2.0f * rng_next_f01(rng);
  return result;
}

#endif //MPLAYER_RANDOM_H
