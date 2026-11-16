# Dynamic voice allocation — design notes

Status: investigation / proposal, not implemented.

This document looks at replacing the current static per-timbre voice reservation
with a single global pool of 16 voices that timbres borrow on demand. It also
looks specifically at the cost of binding a voice to a timbre, because that
cost moves onto the note-on path in a dynamic allocator.

## Goal

- Keep **16 global voices** and **6 timbres**.
- Stop permanently reserving a fixed number of voices per timbre.
- Let any timbre borrow any free global voice when it needs to play a note.
- Preserve poly, mono, unison, MPE, arpeggiator, voice stealing, FX and the
  existing mixer/preset model as much as possible.
- Avoid making note-on or the audio callback more expensive than necessary.

---

## 1. Current allocation model

### 1.1 Ownership

`Synth` owns the global arrays:

- `Voice voices_[MAX_NUMBER_OF_VOICES]` — `Synth.h:158`
- `Timbre timbres_[NUMBER_OF_TIMBRES]` — `Synth.h:159`
- `MAX_NUMBER_OF_VOICES = 16`, `NUMBER_OF_TIMBRES = 6` — `Common.h:34-36`

Each `Timbre` owns:

- `int8_t voiceNumber_[MAX_NUMBER_OF_VOICES]` — `Timbre.h:129`
- `Voice *voices_[MAX_NUMBER_OF_VOICES]` — `Timbre.h:290`
- `float numberOfVoices_` — `Timbre.h:287`

`voiceNumber_[v]` is the **global voice index** used by timbre voice slot `v`.
`voices_[n]` points to the global `Voice` with index `n`. In practice both
arrays are indexed the same way, and many methods do
`voices_[voiceNumber_[k]]`.

### 1.2 Initialization

`Synth::init()` (`Synth.cpp:46-84`) does:

1. copy the default preset into each timbre;
2. call `Timbre::init()` for each timbre;
3. call `Timbre::initVoicePointer(v, &voices_[v])` for every timbre and every
   global voice;
4. call `Voice::init()` on all 16 voices;
5. call `Synth::rebuidVoiceAllTimbre()`;
6. call `Timbre::numberOfVoicesChanged()` with the mixer value for each timbre.

`Synth::rebuidVoiceAllTimbre()` (`Synth.cpp:777-791`) walks the timbres in
order and assigns contiguous global voices:

```cpp
int voiceNumber = 0;
for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
    int nv = mixerState.instrumentState_[t].numberOfVoices;
    for (int v = 0; v < nv; v++) {
        timbres_[t].setVoiceNumber(v, voiceNumber++);
    }
    for (int v = nv; v < MAX_NUMBER_OF_VOICES; v++) {
        timbres_[t].setVoiceNumber(v, -1);
    }
}
```

`Timbre::setVoiceNumber()` (`Timbre.cpp:302-307`) writes the mapping and, for
`n >= 0`, calls `voices_[n]->setCurrentTimbre(this)`.

So the current model is a **static partition**: a timbre owns a contiguous
range of global voices, and no other timbre can use them.

### 1.3 Changing the number of voices

The mixer value is handled by `Synth::newMixerValue()`
(`Synth.cpp:689-760`), case `MIXER_VALUE_NUMBER_OF_VOICES`:

- increasing: `setVoiceNumber(v, getFreeVoice())` for each new slot;
- decreasing: `voices_[voiceNumber_[v]]->killNow()` and
  `setVoiceNumber(v, -1)` for each removed slot;
- then `timbres_[timbre].numberOfVoicesChanged(newValue)`.

`Synth::getFreeVoice()` (`Synth.cpp:165-204`) marks every voice used by every
timbre and returns the first unused global index.

`Synth::getNumberOfFreeVoicesForThisTimbre()` (`Synth.cpp:475-483`) returns
`16 - sum(numberOfVoices of all other timbres)`.

The mixer UI uses the same calculation to cap the value:
`FMDisplayMixer.cpp:850-885` starts from `MAX_NUMBER_OF_VOICES` and subtracts
the other timbres' counts. The sum of all per-timbre counts therefore cannot
exceed 16.

The default mixer uses `{3, 3, 3, 2, 1, 1}` (`MixerState.cpp:131`), which is
13 voices; three voices are already unused by default.

### 1.4 Per-timbre note allocation and stealing

`Timbre::preenNoteOn()` (`Timbre.cpp:360-482`) only scans its own assigned
voices:

```cpp
int iNov = playMode == PLAY_MODE_POLY ? numberOfVoices_ : 1;
for (int k = 0; k < iNov; k++) {
    int n = voiceNumber_[k];
    ...
}
```

It prefers the same note, then a free voice, then the oldest released voice,
then the oldest voice. This is a **per-timbre** stealing policy; a timbre never
takes a voice from another timbre.

`Timbre::preenNoteOff()` (`Timbre.cpp:492-545`) and the render loops
(`Timbre::voicesNextBlock()` — `Timbre.cpp:645-730`,
`Timbre::voicesToTimbre()` — `Timbre.cpp:2490-2529`) also iterate
`numberOfVoices_` and `voiceNumber_`.

The same fixed-list iteration appears in:

- `Timbre::afterNewParamsLoad()` — `Timbre.cpp:2561-2585`
- `Timbre::setNewEffecParam()` — `Timbre.cpp:2638-2643`
- `Timbre::lfoValueChange()` — `Timbre.cpp:2930`
- `Timbre::setMatrixSource()` / `setMatrixPolyAfterTouch()` —
  `Timbre.cpp:3073-3099`
- MIDI clock methods — `Timbre.cpp:3043-3074`
- `Timbre::stopPlayingNow()` — `Timbre.cpp:549-560`

All of these would need to use the timbre's **current active voice list**
instead of a fixed reserved list.

### 1.5 MPE and unison assumptions

MPE:

- `Timbre::noteOnMPE()` and `noteOffMPE()` use
  `voiceNumber_[channel - 1]` (`Timbre.cpp:329-355`).
- This assumes a fixed channel-to-voice mapping: channel 0 is the MPE master
  channel and member channels map to voice slots.
- In Mixer > MIDI, the timbre-1 channel is displayed as `1-X` where
  `X = 1 + numberOfVoices` (`FMDisplayMixer.cpp:448-456`), and the encoder is
  currently ignored for timbre 0 while MPE is active
  (`FMDisplayMixer.cpp:846-849`).
- Outside MPE, the channel is editable from All/1..16
  (`midiButtonState`, `FMDisplayMixer.cpp:120-127`).

Unison:

- Current `PLAY_MODE_UNISON` (`Common.h:275-278`) means “use every voice
  reserved for this timbre”:
  - `Timbre::preenNoteOn()` loops over `numberOfVoices_` (`Timbre.cpp:395-460`);
  - `Timbre::voicesNextBlock()` accumulates all of them into voice 0
    (`Timbre.cpp:645-730`);
  - `Timbre::preenNoteOff()` scans all of them (`Timbre.cpp:492-545`);
  - `Timbre::voicesToTimbre()` copies only voice 0 for unison
    (`Timbre.cpp:2490-2529`);
  - `numberOfVoiceInverse_` is set by `numberOfVoicesChanged()` and used for
    unison detune.

In a dynamic pool this would make unison take every voice currently owned by
the timbre, and potentially many voices from the global pool. Unison should
instead use a fixed width, independent of the timbre's maximum voice count.

A dynamic allocator must handle MPE and unison explicitly; neither can simply
use a first-free-voice rule.

---

## 2. Cost of binding a voice to a timbre

### 2.1 Where it happens today

Binding happens through:

```text
Synth::rebuidVoiceAllTimbre()
  -> Timbre::setVoiceNumber(v, n)
     -> Voice::setCurrentTimbre(this)
```

and through `Synth::newMixerValue()` when increasing the number of voices:

```text
Synth::newMixerValue(MIXER_VALUE_NUMBER_OF_VOICES)
  -> Timbre::setVoiceNumber(v, Synth::getFreeVoice())
     -> Voice::setCurrentTimbre(this)
```

So the current cost is paid only when:

- the firmware starts (`Synth::init()`);
- a mixer bank is loaded (`Synth::afterNewMixerLoad()`, `Synth.cpp:531-560`);
- the user changes the number of voices for a timbre.

It is not currently on the per-note path.

### 2.2 What `Voice::setCurrentTimbre()` does

`Voice::setCurrentTimbre()` is `Voice.cpp:4043-4090`. It:

1. stores `this->currentTimbre = timbre`;
2. calls `matrix.init(&timbre->params_.matrixRowState1)`, which only stores a
   pointer (`Matrix.cpp:26-28`);
3. reinitializes three `LfoOsc` objects:
   - `LfoOsc::init()` sets type, ramp, phase pointer, params pointer, source,
     destination;
   - calls `valueChanged(3)`;
   - calls `midiClock(0, true)`, which selects a clock mode and updates
     `currentFreq` from `invTab` (`LfoOsc.cpp:24-70`);
4. reinitializes `LfoEnv` and `LfoEnv2`:
   - each calls `valueChanged(0..3)`;
5. reinitializes two `LfoStepSeq` objects:
   - `valueChanged(0)` and `midiClock(0, true)`;
6. calls `setNewEffectParam(0)`;
7. initializes six pan left/right pairs from `panTable`;
8. sets `mixerGain`.

`Voice::setNewEffectParam(0)` (`Voice.cpp:7881-7908`) resets all filter state
and recursively calls encoders 1..3. It depends on
`currentTimbre->params_.effect1.type` and effect parameters.

The LFO `valueChanged()` and `midiClock()` work is the main part of the
rebinding cost. It is not huge, but it is much more than a pointer swap.

### 2.3 Code-size observation

In the current firmware build:

```text
Voice::setCurrentTimbre(Timbre*)   656 bytes
Voice::init()                       28 bytes
Timbre::setVoiceNumber(int, int)    24 bytes
```

`Voice::init()` is cheap; the rebinding work is in `setCurrentTimbre()`.

This is code size, not execution time. It does show that rebinding is a real
function with several initializers, not a single assignment.

### 2.4 Decision: precompute first

Do not add a DWT measurement phase before implementing dynamic allocation. Go
directly to the per-timbre `TimbreVoiceContext` described in section 3 and use
it for the first implementation.

The goal is to make rebinding cheap enough that per-note binding is not a
concern. If a real performance problem appears later, it can be investigated
then; there is no measurement gate before coding.

### 2.5 Critical ordering: quick note-off must use the old timbre

Same-timbre stealing already relies on a deferred note start:

- `Timbre::preenNoteOn()` chooses `voiceToUse` and calls
  `Voice::noteOnWithoutPop()` for `NEW_NOTE_OLD` / `NEW_NOTE_RELEASE`
  (`Timbre.cpp:420-440`).
- `Voice::noteOnWithoutPop()` (`Voice.cpp:420-451`) sets `newNotePending`,
  stores the pending note/velocity/frequency, and calls
  `env1_.noteOffQuick()` through `env6_.noteOffQuick()` on the **current**
  timbre.
- The pending note is not started immediately. `Voice::nextBlock()` calls
  `Voice::endNoteOrBeginNextOne()` (`Voice.cpp:546-563`) once the quick release
  has finished; that is where `noteOn(pendingNote, ...)` happens.

This is the anti-click mechanism: the old waveform is allowed to decay to zero
with the old envelopes before the new note starts.

For cross-timbre stealing this means:

- do **not** call `setCurrentTimbre()` / `bindTimbre()` at steal time;
- do **not** remove the voice from the old timbre's active list yet;
- store the target timbre/context in `Voice::pendingTimbre_` /
  `Voice::pendingContext_`;
- let the old timbre keep rendering the voice during the quick release;
- in `endNoteOrBeginNextOne()`, after the quick release has finished and
  immediately before `noteOn(pendingNote, ...)`, bind the voice to the pending
  timbre and transfer ownership to the new timbre's active list;
- only then start the pending note.

If ownership is moved before the quick release completes, the old timbre's
`voicesNextBlock()` stops rendering the voice, so `endNoteOrBeginNextOne()` may
never run and the quick release is cut off. If `currentTimbre` is changed
before the quick release, the old note decays through the new timbre's
envelopes/oscillators, which is exactly the click/wrong-sound case to avoid.

Matrix sources are also per-voice. `Timbre::preenNoteOn()` currently calls
`preenNoteOnUpdateMatrix()` before `noteOnWithoutPop()`. For a cross-timbre
steal, the matrix source update should be deferred with the timbre binding (or
performed only after the quick release), otherwise the old note's tail can see
the new note's note/velocity sources.

### 2.6 MSFA-specific binding

MSFA's per-voice state is a `Dx7Note` (`source/msfa/dx7note.h`):

- six `Env` objects, six `FmOpParams`, a `PitchEnv`, `basepitch_[6]`, feedback
  state, operator modes/switch and controller state;
- `Dx7Note::init(patch, note, velocity, channel, ctrls)`
  (`dx7note.cc:163-214`) initializes all six envelopes/operators and is
  normally part of note-on;
- `Dx7Note::update(patch, note, velocity, channel)` (`dx7note.cc:364-404`)
  recalculates all six envelopes/operators and is the cost of applying a patch
  change to an already active voice.

So the answer depends on how binding is implemented:

- If binding a voice to an MSFA timbre only stores the timbre's
  `MsfaProgram*`/context and the next note-on calls `Dx7Note::init()`, the
  binding is likely **cheaper** than PFM3 `setCurrentTimbre()`: there are no
  PFM3 LFO, matrix, pan or effect reinitializations.
- If an already-active voice must be rebound with `Dx7Note::update()` for all
  six operators, the cost can be similar to or higher than PFM3 binding.
- `Dx7Note` also carries a `std::shared_ptr<TuningState>` and Dexed's tuning
  path (`MTS_NoteToFrequency`, `osc_freq`). Avoid the MTS path in firmware
  unless microtuning is required.
- Use the bind-on-note-on design: store the patch/context when claiming the
  voice and let the next note-on call `Dx7Note::init()`. Do not use
  `Dx7Note::update()` for active-voice rebinding in the first implementation.

The earlier “MSFA may be more expensive” note was a cautious assumption, not a
finding. For the bind-on-note-on design the opposite is likely true.

---

## 3. Precomputing per-timbre values

We are going with the precomputed per-timbre context directly. Most of
`setCurrentTimbre()` depends only on the timbre's parameters, not on the voice.
Those values can be computed once per timbre and reused by every voice.

### 3.1 Proposed `TimbreVoiceContext`

Add a small struct owned by `Timbre`, recomputed when the preset/parameters
change:

```cpp
struct TimbreVoiceContext {
    struct MatrixRowParams *matrixRows;
    struct LfoParams *lfoOscParams[3];
    float *lfoPhases[3];
    struct EnvelopeLfoParams *lfoEnv;
    struct Envelope2LfoParams *lfoEnv2;
    struct StepSequencerParams *lfoStepSeqParams[2];
    struct StepSequencerSteps *lfoStepSeqSteps[2];

    const struct AlgoInformation *algoInfo;
    const int *algoOpInfo;
    int numberOfIMs;
    int numberOfMix;

    float panLeft[6];
    float panRight[6];
    float mixerGain;
};
```

`Timbre` would update this context in:

- `Timbre::afterNewParamsLoad()` — `Timbre.cpp:2561-2585`;
- `Timbre::setNewValue()` when `algo`, pan, effect or LFO params change;
- `Timbre::updateMidiNoteScale()` if it affects cached values.

### 3.2 Use in `Voice`

`Voice` would keep a pointer:

```cpp
const TimbreVoiceContext *timbreContext_;
```

`Voice::setCurrentTimbre()` becomes roughly:

```cpp
void Voice::bindTimbre(const TimbreVoiceContext *ctx, Timbre *timbre) {
    currentTimbre = timbre;
    timbreContext_ = ctx;
    matrix.init(ctx->matrixRows);
    lfoOsc[0].bind(ctx->lfoOscParams[0], ctx->lfoPhases[0], ...);
    // ...
    memcpy(panLeft, ctx->panLeft, sizeof(panLeft));
    memcpy(panRight, ctx->panRight, sizeof(panRight));
    mixerGain = ctx->mixerGain;
    resetPerVoiceState();       // filter/LFO/envelope state
}
```

The goal is to avoid recomputing pan tables, algorithm metadata and LFO
configuration for every voice.

### 3.3 What can and cannot be shared

Can be precomputed per timbre:

- matrix row pointer;
- LFO parameter/phase pointers and static LFO setup;
- algorithm metadata (`algoInformation[algo].im/mix`,
  `algoOpInformation[algo]`);
- pan table results for the six oscillators;
- effect type/coefficients and mixer gain.

Must stay per voice:

- envelope state (`EnvData envState1_..6_`);
- oscillator phase state (`OscState oscState1_..6_`);
- matrix source/destination values;
- LFO per-voice counters/phases if they are voice-specific;
- filter state (`v0L..v8L`, `v0R..v8R`, `fxParam*`);
- note/glide/velocity state.

The existing `Matrix` already separates its `rows` pointer from its
source/destination arrays, so only the pointer needs rebinding.

### 3.4 Cache algorithm metadata

`Voice::updateAllModulationIndexes()` (`Voice.h:106-149`) reads
`algoInformation[algo].im` every block. `Voice::updateAllMixOscsAndPans()`
(`Voice.h:151-...`) reads `algoInformation[algo].mix` every block. Those
lookups are cheap but repeated 16 times per block.

Caching `algoInfo`/`algoOpInfo` in the timbre context removes the array index
and makes the per-block code slightly smaller. The cache must be invalidated
when `ROW_ENGINE`/algorithm changes.

---

## 4. Allocator design

### 4.1 Ownership

Use a dedicated `VoicePool` class, owned by `Synth`. `Synth` owns the 16
`Voice` objects and gives the pool to the timbres; `Timbre` asks the pool for
voices and never manipulates global indices directly.

```cpp
class VoicePool {
public:
    int allocate(uint8_t timbre, uint8_t note, uint8_t velocity);
    void release(int voiceIndex);
    void releaseAll(uint8_t timbre);
    int owner(int voiceIndex) const;
private:
    int8_t owner_[MAX_NUMBER_OF_VOICES];   // -1 = free
    uint32_t startIndex_[MAX_NUMBER_OF_VOICES];
};
```

`Synth` exposes the pool (or small `allocateVoiceForTimbre()` wrappers).
`Timbre` should not need to know about global indices except through the pool.

### 4.2 Per-timbre active list

Replace the fixed reservation with an active list:

```cpp
int8_t activeVoices_[MAX_NUMBER_OF_VOICES]; // global indices
uint8_t activeVoiceCount_;
uint8_t maxVoices_; // per-timbre maximum from mixer (Option A)
```

`voiceNumber_` can be repurposed as this active list, or replaced by
`activeVoices_` to make the new meaning explicit.

`numberOfVoices_` currently means both "reserved count" and "number of voices
to render". In the dynamic model, keep `maxVoices_` as the configured
per-timbre maximum from the mixer and use a separate active list/count for what
is currently playing.

### 4.3 Mixer semantics

Decision: **Option A**. `numberOfVoices` is a per-timbre maximum, not a
reservation.

- Each timbre can be set from 0 to 16 independently. The sum of the six maxima
  may exceed 16.
- The only hard limit is the physical pool: at most 16 voices can be active at
  the same time.
- If timbre 2 is playing alone with a maximum of 16, it can use all 16 voices.
  When other timbres start playing, they share the same pool and the allocator
  steals voices as needed.
- The mixer UI must stop subtracting the other timbres' counts from the
  maximum. Every timbre can be set up to 16.
- `maxVoices_` is the configured maximum, used by the unison width and by MPE
  X. A separate active list/count tracks what is currently playing.
- `Synth::getFreeVoice()` and
  `Synth::getNumberOfFreeVoicesForThisTimbre()` are replaced by `VoicePool`.
  `Synth::newMixerValue()` updates the maximum only.
- Lowering the maximum below the current active count kills the excess voices,
  as today.
- No minimum guaranteed voice count is reserved. Users are expected to manage
  the 16-voice limit; global stealing handles contention.

### 4.4 Allocation / stealing policy

Suggested order:

1. **Same-note retrigger** within the timbre, if the voice is still owned by
   that timbre.
2. **Free voice** in the global pool.
3. **Oldest released voice in the global pool**, regardless of timbre.
4. **Oldest playing voice owned by the same timbre**.
5. **Oldest playing voice in the global pool**.

Why released voices come before same-timbre playing voices: taking a voice
that is already in release only truncates a fading tail. Taking a held voice
is more audible. This avoids cutting a held note in the requesting timbre while
an idle/releasing voice exists elsewhere.

Rules:

- With Option A, never exceed the target timbre's `maxVoices_`. The sum of the
  per-timbre maxima may exceed 16; only the global active pool is limited to
  16.
- Prefer any released voice over any playing voice.
- After free/released voices are exhausted, prefer stealing a playing voice
  from the requesting timbre before stealing another timbre's held note.
- No minimum guaranteed voice count is reserved. A timbre can be fully stolen
  by other timbres; that is accepted.
- On cross-timbre steal, keep the voice bound to the **old** timbre and call
  `Voice::noteOnWithoutPop()` so the quick release uses the old envelopes.
  Do not call `bindTimbre()` or move the active-list entry yet.
- Store the target timbre/context as `Voice::pendingTimbre_` /
  `pendingContext_`. The old timbre remains the renderer until
  `endNoteOrBeginNextOne()` completes the quick release.
- At the end of the quick release, transfer the voice to the new timbre's
  active list, bind the new context, and start the pending note. This is the
  only safe ordering for avoiding a click or a wrong-timbre tail.
- Do not update the matrix sources for the new note before the quick release
  has finished.
- Do not steal a voice reserved by a unison group or an MPE channel unless
  there is no alternative.

### 4.5 Note-off routing

`Timbre::preenNoteOff()` must search the timbre's active list and match the
note, not use a fixed `voiceNumber_[k]` range.

If a voice has been stolen by another timbre, the old timbre must not receive
or act on its note-off. The allocator's `owner_` and the voice's current
timbre pointer should be checked before sending note-off.

During the deferred quick release there are two notes in flight:

- the old note, still rendered by the old timbre, which must ignore any
  note-off because it is already being stolen;
- the pending new note, which belongs to the target timbre but has not started
  yet.

The target timbre therefore needs to track the pending voice (for example a
small `pendingVoices_` list or a `Voice::pendingTimbre_` check) so that a
note-off arriving before the new note starts reaches `Voice::noteOff()`.
`Voice::noteOff()` already handles the `newNotePending` case by flagging
`pendingNote += 128` (`Voice.cpp:596-599`). Ownership and note-off routing must
use the pending target, not only `currentTimbre`.

### 4.6 MPE

Current behavior:

- MPE applies to instrument 1 only. `MPE_inst1_` is configured from
  Mixer > Global (`FMDisplayMixer.cpp:181`).
- In MPE mode, the timbre-0 MIDI channel value is not used for note routing.
  `MidiDecoder::midiEventForInstrument1MPE()` (`MidiDecoder.cpp:350-418`)
  treats channel 0 as the master channel and sends member-channel note events
  to `Timbre::noteOnMPE(midiEvent.channel, ...)`.
- `Timbre::noteOnMPE()` / `noteOffMPE()` map the member channel to
  `voiceNumber_[channel - 1]` (`Timbre.cpp:329-355`). Channel 0 is the master;
  member channel 1 uses voice slot 0, channel 2 uses slot 1, and so on.
- Mixer > MIDI displays the timbre-1 channel as `1-X`, where
  `X = 1 + numberOfVoices` (`FMDisplayMixer.cpp:448-456`), and the encoder is
  ignored for timbre 0 in MPE mode (`FMDisplayMixer.cpp:846-849`).
- Outside MPE, the channel is editable from All/1..16
  (`midiButtonState`, `FMDisplayMixer.cpp:120-127`).

Decision: the editable Mixer > MIDI value is the **last member channel
range limit** `[1..X]`.

- The MPE master channel remains channel 1.
- Member channels are `2..X`.
- With Option A, X defaults to `min(16, 1 + maxVoices_)` for timbre 1 and is
  clamped to `[1, min(16, 1 + maxVoices_)]`. Turning the first Mixer > Voices
  encoder (timbre 1) changes `maxVoices_`, which changes the maximum X; if the
  stored X is above the new maximum, clamp it.
- If `maxVoices_` is 16, X is capped at 16: the master plus 15 member channels,
  so one voice slot is not addressable through MPE.
- Lowering X disables the member channels above X; MPE note routing must ignore
  those channels.
- When X is lowered while notes are active on channels above X, kill those
  voices immediately with `Voice::killNow()`, exactly like reducing the
  timbre's voice count today (`Synth::newMixerValue()`, `Synth.cpp:730-736`).
  This is not a live-performance operation; an audible click is acceptable.
- Store X in a dedicated field, for example `MixerInstrumentState::mpeLastChannel`
  (or a timbre-0-only field in `MixerState`), and persist it with the mixer
  bank.
- The display remains `1-X`, but X is now the stored value instead of always
  `1 + numberOfVoices`.

Dynamic allocation requirements:

- Replace `voiceNumber_[channel - 1]` with an explicit per-timbre
  `mpeChannelVoice_[16]` map (or equivalent) from member channel to global
  voice index.
- Allocate a voice on the first note for a channel and keep the mapping until
  note-off/channel release. If the voice is stolen, clear or update the old
  channel mapping.
- Ignore MPE member channels above the stored X in `Timbre::noteOnMPE()` /
  `noteOffMPE()` and in `MidiDecoder`.
- With Option A, when `maxVoices_` changes, recompute the maximum X and clamp
  the stored value. Do not orphan active notes or point two channels at the
  same voice.
- Preserve the current master-channel behavior and MPE pitch bend, aftertouch
  and CC74 routing (`MidiDecoder.cpp:355-416`).
- Size the MPE channel map for 16 channels. The effective X maximum is
  `min(16, 1 + maxVoices_)`.

This must be designed before enabling dynamic allocation for MPE timbres.

### 4.7 Unison

Today `PLAY_MODE_UNISON` means “use every voice reserved for this timbre”.
`Timbre::preenNoteOn()` loops over `numberOfVoices_`, `voicesNextBlock()`
accumulates all of them, `preenNoteOff()` scans all of them, and
`numberOfVoiceInverse_` is `1 / numberOfVoices_`. In a dynamic pool this would
mean unison tries to take all voices currently owned by the timbre and can
monopolize the global pool.

Recommendation: use Option A from 4.3. The unison width is the timbre's
configured `numberOfVoices`/`maxVoices_`; no separate `Unis3`/`Unis5` enum is
needed.

- `PLAY_MODE_UNISON` remains the mode; `isUnisonMode()` stays
  `playMode == PLAY_MODE_UNISON`.
- If `maxVoices_` is 3, unison uses 3 voices. If it is 5, unison uses 5.
- If it is 16 and the timbre is alone, unison can use all 16 voices.
- If other timbres need voices, the allocator steals according to the global
  policy. If the full unison width cannot be obtained, skip the note rather
  than playing a narrower group.
- `numberOfVoiceInverse_` is `1 / maxVoices_` while the unison group is active.
- Lowering `maxVoices_` below the active unison group kills the excess voices
  immediately, as today.
- If a separate unison width is wanted later, add it as a new parameter; do not
  expand the play-mode enum now.

Allocator behavior:

- `allocateUnison(timbre, maxVoices_, outVoices)` claims exactly that many
  voices atomically.
- If there are not enough free voices, use the normal steal policy.
- If there are still not enough, skip the note. Do not silently play a
  different width.
- `numberOfVoiceInverse_` must be `1 / maxVoices_` (or `1 / actualClaimed`)
  while the unison group is active.

Implementation points:

- `Timbre::preenNoteOn()` — `Timbre.cpp:360-482`: use `maxVoices_` for the
  unison width and the allocator claim.
- `Timbre::preenNoteOff()` — `Timbre.cpp:492-545`: iterate the active unison
  group, not the fixed reserved list.
- `Timbre::voicesNextBlock()` — `Timbre.cpp:645-730`: accumulate the active
  unison group and keep the first voice as the mix target.
- `Timbre::voicesToTimbre()` — `Timbre.cpp:2490-2529`: still copy only voice 0
  for unison.
- `Timbre::numberOfVoicesChanged()` — `Timbre.h:88-96`: update
  `numberOfVoiceInverse_` from `maxVoices_`.
- `Timbre::stopPlayingNow()` — `Timbre.cpp:549-560`: release the active unison
  group.
- `Timbre::isUnisonMode()` — `Timbre.h:156`: unchanged
  (`playMode == PLAY_MODE_UNISON`).

Preset migration: no preset change. The unison width comes from the mixer
`numberOfVoices` setting, which is already persisted in the mixer bank. The
play-mode value stays 2 for unison.

Decision: unison width is always `maxVoices_`. If the full width cannot be
claimed, skip the note; do not silently play a narrower group.

### 4.8 Arpeggiator

The arpeggiator drives `Timbre::noteOn()`/`noteOff()` through the same path, so
the allocator can handle it. The risk is that an arpeggiated note may be
stolen while the arpeggiator still expects it to exist. Note-off matching by
note and owner should make this safe, but it needs testing with latch/hold.

---

## 5. Files and methods to change

### `Synth`

- `Synth::init()` — `Synth.cpp:46-84`: allocate the global pool instead of
  partitioning.
- Add `VoicePool voicePool_` to `Synth`; give each `Timbre` a pointer/reference
  to it.
- `Synth::rebuidVoiceAllTimbre()` — `Synth.cpp:777-791`: remove; the
  `VoicePool` replaces the static mapping.
- `Synth::getFreeVoice()` — `Synth.cpp:165-204`: replace with allocator.
- `Synth::getNumberOfFreeVoicesForThisTimbre()` — `Synth.cpp:475-483`:
  remove; the allocator owns the free-voice logic.
- `Synth::newMixerValue()` — `Synth.cpp:689-760`: the
  `MIXER_VALUE_NUMBER_OF_VOICES` case updates `maxVoices_`; it no longer
  reserves or releases voice slots.
- `Synth::afterNewMixerLoad()` — `Synth.cpp:531-560`: clear/rebuild active
  lists.
- `Synth::beforeNewParamsLoad()` — `Synth.cpp:486-528`: iterate active list.
- `Synth::allNoteOff*()` — `Synth.cpp:155-205`: iterate active list.

### `Timbre`

- `Timbre::initVoicePointer()` — `Timbre.cpp:309-311`: likely removable.
- `Timbre::setVoiceNumber()` — `Timbre.cpp:302-307`: replace with
  `claimVoice()` / `releaseVoice()`.
- `Timbre::preenNoteOn()` — `Timbre.cpp:360-482`: ask allocator for a voice
  instead of scanning reserved slots; use `maxVoices_` for unison width.
- `Timbre::preenNoteOff()` — `Timbre.cpp:492-545`: search active list and the
  active unison group.
- `Timbre::voicesNextBlock()` — `Timbre.cpp:645-730`: iterate active list and
  accumulate the active unison group (`maxVoices_`).
- `Timbre::voicesToTimbre()` — `Timbre.cpp:2490-2529`: iterate active list;
  keep the unison voice-0 mix behavior.
- `Timbre::afterNewParamsLoad()` — `Timbre.cpp:2561-2585`: iterate active list.
- `Timbre::setNewEffecParam()` — `Timbre.cpp:2638-2643`.
- `Timbre::lfoValueChange()` — `Timbre.cpp:2930`.
- `Timbre::setMatrixSource()` / `setMatrixPolyAfterTouch()` —
  `Timbre.cpp:3073-3099`.
- MIDI clock methods — `Timbre.cpp:3043-3074`.
- `Timbre::stopPlayingNow()` — `Timbre.cpp:549-560`: release the active unison
  group.
- `Timbre::numberOfVoicesChanged()` — `Timbre.h:88-96`: set `maxVoices_` and
  compute `numberOfVoiceInverse_` from `maxVoices_`.

### `Voice`

- Add `pendingTimbre_`, `pendingContext_` and a `stealPending_` flag.
- `Voice::init()` — `Voice.cpp:390-400`: keep cheap; clear pending binding.
- `Voice::noteOnWithoutPop()` — `Voice.cpp:420-451`: keep using the **current**
  timbre for `noteOffQuick()`, but store the target timbre/context as pending
  when the voice is being stolen from another timbre.
- `Voice::endNoteOrBeginNextOne()` — `Voice.cpp:546-563`: after the quick
  release finishes, apply the pending timbre/context and transfer ownership,
  then call `noteOn(pendingNote, ...)`.
- `Voice::noteOff()` — `Voice.cpp:572-600`: keep the `newNotePending` handling
  and make sure a pending note-off is routed to the target timbre.
- `Voice::killNow()` — `Voice.cpp:628-642`: clear the pending timbre/context
  and any steal flag.
- `Voice::setCurrentTimbre()` — `Voice.cpp:4043-4090`: replace with
  `bindTimbre(const TimbreVoiceContext*, Timbre*)`.
- `Voice::setNewEffectParam()` — `Voice.cpp:7881-7908`: use cached effect
  values where possible.
- `Voice::afterNewParamsLoad()` — `Voice.h:338-...`: keep per-voice reset.
- `Voice::nextBlock()` / `fxAfterBlock()` — use `currentTimbre` as today.

### Mixer / UI

- `MixerInstrumentState::numberOfVoices` — `MixerState.h:48`: keep it as the
  per-timbre maximum. Old values are reinterpreted as maxima; a format change
  is not strictly required.
- `MixerState::getFullState()` / `restoreFullStateVersion*()` —
  `MixerState.cpp:46-520`: keep the field. Bump the version only if a migration
  needs to distinguish old reserved-count semantics.
- `FMDisplayMixer` “Voices” button/state — `FMDisplayMixer.cpp:106-115`: keep
  it; it now edits the maximum polyphony.
- `FMDisplayMixer::encoderTurned()` — `FMDisplayMixer.cpp:850-885`: allow every
  timbre 0..16 independently. Remove the `16 - sum(other timbres)` cap; the sum
  of maxima may exceed 16.
- No `PLAY_MODE_*` enum expansion, no `polyMonoNames` change and no
  `engine1ParameterRow` change: unison width comes from `maxVoices_`.
- `FMDisplayEditor` display filtering — `FMDisplayEditor.cpp:3446-3454`: keep
  using `playMode == PLAY_MODE_UNISON` to show spread/detune.

### MPE

- `MidiDecoder::midiEventForInstrument1MPE()` — `MidiDecoder.cpp:350-418`:
  keep the master/member routing; ignore member channels above the stored X.
- `Timbre::noteOnMPE()` / `noteOffMPE()` — `Timbre.cpp:329-355`: replace
  `voiceNumber_[channel - 1]` with the explicit MPE channel map and reject
  member channels above X.
- `Timbre::setMatrixSourceMPE()` — `Timbre.cpp:3084`: keep the per-channel
  pitch bend/aftertouch/slide routing, but ignore channels above X.
- `FMDisplayMixer::displayMixerValue()` — `FMDisplayMixer.cpp:448-456`: the
  `1-X` MPE display must use the stored `mpeLastChannel`.
- `FMDisplayMixer::encoderTurned()` — `FMDisplayMixer.cpp:846-849`: remove the
  early return so X is editable; clamp it to
  `[1, min(16, 1 + maxVoices_)]`.
- Add `MixerInstrumentState::mpeLastChannel` (or a timbre-0-only
  `MixerState::mpeLastChannel_`) and persist it. Bump `MIXER_BANK_VERSION` if
  the mixer layout changes. When X is lowered, kill the voices on removed
  channels with `Voice::killNow()`.

### Preset / storage

No preset format change is required for the unison width: it comes from the
mixer `numberOfVoices` setting, which is persisted in the mixer bank. The
play-mode value stays `2` for unison, and no new `Unis*` values are added.

The mixer bank keeps its existing `numberOfVoices` field, but its meaning
changes from reserved count to maximum. Old banks load with those values as
maxima. Bump `MIXER_BANK_VERSION` only if a migration needs to distinguish the
old semantics.

---

## 6. Migration plan

1. **Extract `TimbreVoiceContext`.**
   - Add the struct and precompute it in `Timbre::afterNewParamsLoad()` and
     parameter changes.
   - Make `Voice::setCurrentTimbre()` use it.
   - Verify no audio or behavior change with the existing static allocation.

2. **Add the global `VoicePool`.**
   - Implement `VoicePool` and an active voice list.
   - Replace the static reservation path directly; do not keep a runtime
     fallback.

3. **Move note-on/off and render loops to active lists.**
   - Update every `numberOfVoices_`/`voiceNumber_` loop listed above.
   - Keep `maxVoices_` separate from the active count.

4. **Change mixer semantics and persistence.**
   - Treat `numberOfVoices` as max polyphony, 0..16 per timbre independently.
   - Remove the `16 - sum(other timbres)` UI cap; the sum of maxima may exceed
     16.
   - No mixer format change is strictly required; old values become maxima.

5. **Handle MPE, unison, arpeggiator and voice stealing explicitly.**
   - Add tests before enabling these modes on dynamic allocation.

6. **Remove the static reservation code.**
   - Delete `rebuidVoiceAllTimbre()` and the fixed `voiceNumber_` mapping as
     part of the switch. No compatibility mode or fallback path.

---

## 7. Test plan

- 6 timbres, all playing at once, with more than 16 simultaneous notes.
- One timbre holds all 16 voices; a note on another timbre must be able to
  steal the oldest voice. Verify there is no permanent starvation.
- Set several timbres to maximum 16 at the same time. The sum of maxima may
  exceed 16; only the active voice count must stay at or below 16.
- No minimum reservation: a timbre may be fully stolen by other timbres; verify
  that this is accepted behavior and no voice is leaked.
- Load mixer banks saved with the old `numberOfVoices` field and confirm the
  values load as per-timbre maxima.
- Mixed engines later: PFM3 and MSFA timbres competing for the same 16 voices.
- Poly, mono, unison, glide, hold pedal.
- Unison width equals `maxVoices_`: set max 3/5/8 and verify each unison note
  claims exactly that many voices.
- Unison with insufficient free voices: verify the steal/skip policy, that no
  voice is leaked, and that `numberOfVoiceInverse_` matches the width.
- Lowering `maxVoices_` while a unison note is playing kills the excess voices
  immediately.
- Unison width persistence through mixer bank save/load.
- MPE channels, including note-off, channel reuse, and editing the MPE
  last-member-channel limit X while MPE is active.
- Raising/lowering X: channels above X must be ignored for note-on/off,
  pitch bend, aftertouch and CC74; X must persist in the mixer bank.
- Lowering X while notes are active above X must kill those voices
  immediately (`killNow()`), matching the current voice-count reduction
  behavior; an audible click is acceptable.
- Changing `maxVoices_` while MPE notes are active: X must be clamped to
  `min(16, 1 + maxVoices_)`, and the channel-to-voice map rebuilt or clamped
  without orphaned notes or two channels sharing a voice.
- Arpeggiator patterns, latch and hold.
- Voice stealing: same timbre, cross-timbre, released voices, playing voices.
- Cross-timbre steal ordering: the quick release must be rendered entirely by
  the old timbre, the voice must remain in the old active list until
  `endNoteOrBeginNextOne()`, and the new note must start only after rebinding.
- No clicks/pops and no wrong-timbre tail when a voice is rebound to another
  timbre.
- Note-off arriving during the deferred quick release, for both the old and
  the pending new note.
- Mixer save/load, preset load, boot defaults.
- Long soak test with random note storms across all six timbres.

---

## 8. Decisions

1. Mixer semantics: **Option A**. `numberOfVoices` is a per-timbre maximum,
   set 0..16 independently; the sum of maxima may exceed 16.
2. Stealing order: same-note retrigger > global free voice > oldest globally
   released voice > oldest playing voice owned by the same timbre > oldest
   playing voice globally.
3. Unison width: always `maxVoices_`. No separate `Unis*` play-mode values.
4. No minimum guaranteed voice count; users manage the 16-voice limit and
   global stealing handles contention.
5. Allocator: a separate `VoicePool` class owned by `Synth`.
6. Remove `rebuidVoiceAllTimbre()` and the fixed `voiceNumber_` mapping
   directly. No compatibility mode, fallback path or development flag.
7. Cross-timbre stealing keeps the old timbre for the quick release; rebinding
   and ownership transfer happen in `endNoteOrBeginNextOne()`.
8. MPE X is the editable last member channel, clamped to
   `min(16, 1 + maxVoices_)`; lowering X kills voices above it immediately.