/* date = December 18th 2024 1:07 am */

#ifndef HAZE_RANDOM_H
#define HAZE_RANDOM_H

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

#endif //HAZE_RANDOM_H
