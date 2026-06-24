# zCVobSound random-delay scheduling: symmetric `delay ± delayVar`, not `delay + [0,delayVar)`

**Confidence:** High

## Original function + address

`zCVobSound::DoSoundUpdate` (Gothic2.exe `0x0063e210`).

In the RANDOM sound-mode branch (vob sound-type field == 2), when the stored
"next start" timestamp has been reached, the original starts the sound and then
computes the next start timestamp as:

    nextStart = currentTime + ( delay + r * delayVar ) * 1000

where:
- `delay`    is the vob's average random delay (seconds, field at `+0x144`),
- `delayVar` is the vob's random-delay deviation (seconds, field at `+0x148`),
- `r` is `(rand() - 16383.5) * 6.1037e-05`, i.e. `rand()` (range 0..32767)
  remapped to the closed interval **[-1.0, +1.0]** (verified: `16383.5 * 6.1037e-05 == 1.0`).

So the random delay is **symmetric and centered on `delay`**: the actual delay
is drawn uniformly from `[delay - delayVar, delay + delayVar]`, with mean = `delay`.
This matches the ZenKit field documentation itself
(`lib/ZenKit/include/zenkit/vobs/Sound.hh:63`):
"The resulting delay will be a value between `random_delay ± random_delay_var`."

## OpenGothic file:line

`game/world/worldsound.cpp:205-207`

```cpp
i.restartTimeout = owner.tickCount() + i.delay;
if(i.delayVar>0)
  i.restartTimeout += uint64_t(std::rand())%i.delayVar;
```

Fields are populated at `game/world/worldsound.cpp:95-96`:
`i.delay = random_delay*1000`, `i.delayVar = random_delay_var*1000` (both in ms),
matching the original's `*1000` second→ms scaling.

## Divergence

OpenGothic adds a **one-sided** random offset: the next delay is drawn from
`[delay, delay + delayVar)` (using `rand() % delayVar`), never shorter than
`delay`, with mean `delay + delayVar/2`.

The original draws from the **two-sided** interval `[delay - delayVar, delay + delayVar]`
with mean `delay`. Consequences:

- OpenGothic's ambient random sounds repeat on average `delayVar/2` ms **later**
  than the original, and never earlier than `delay`.
- The total spread is halved (`delayVar` wide vs `2*delayVar` wide), so the
  variation feels more regular/clustered than in the original.

This is a numeric/timing parity divergence in the ambient random-delay scheduler.

## Proposed patch

Reproduce the symmetric `delay ± delayVar` distribution. `std::rand()` returns
`[0, RAND_MAX]`; mapping `2.0*rand()/RAND_MAX - 1.0` gives `r ∈ [-1, +1]` as in
the original, then offset by `r*delayVar`. Guard against the result going
negative (when `delayVar > delay`) by clamping to 0 before adding to the tick
count, since `restartTimeout` is an unsigned `uint64_t`.

OLD (`game/world/worldsound.cpp:205-207`):
```cpp
    i.restartTimeout = owner.tickCount() + i.delay;
    if(i.delayVar>0)
      i.restartTimeout += uint64_t(std::rand())%i.delayVar;
```

NEW:
```cpp
    // NOTE: in original-game zCVobSound::DoSoundUpdate (Gothic2.exe 0x0063e210)
    // the next random delay is drawn symmetrically from [delay-delayVar, delay+delayVar]
    // (rand() remapped to [-1,+1] scaled by delayVar), mean == delay. OpenGothic used a
    // one-sided rand()%delayVar -> [delay, delay+delayVar), biasing every ambient repeat
    // ~delayVar/2 ms late and halving the spread.
    int64_t next = int64_t(i.delay);
    if(i.delayVar>0) {
      const double r = 2.0*double(std::rand())/double(RAND_MAX) - 1.0; // [-1,+1]
      next += int64_t(r*double(i.delayVar));
      }
    if(next<0)
      next = 0;
    i.restartTimeout = owner.tickCount() + uint64_t(next);
```

Grep-verified OG symbols used: `i.delay`, `i.delayVar`, `i.restartTimeout`
(`game/world/worldsound.cpp:32-34`), `owner.tickCount()` (used at lines 177, 205,
231-233). `<cstdlib>` for `std::rand`/`RAND_MAX` is already transitively in use
(`std::rand()` at the original line 207).
