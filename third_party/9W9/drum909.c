/* SPDX-License-Identifier: GPL-3.0-only */
/* The 9W9 TR-909 voices on the FM-1. A port of 9W9's er99_engine.c,
 * er99_circuit.h, er99_tom909.h and er99_perc909.h (GPL-3.0): the circuit models,
 * the fitted constants, the defaults and the per-trigger pinning are 9W9's, line
 * for line; only the arithmetic substrate changed (see drum909_dsp.h). Comments
 * that explain WHY a constant is what it is live in 9W9 and are not repeated. */
#define X0X_DRUM_TABLES_DEFINE              /* the shared lookup tables live here (x0x_drum_tables.h) */
#include "drum909.h"
#include "x0x_drum_samples.h"

#define D9_VCA_VT 0.0002f
#define D9_VCA_NORM (1.0f / (1.0f - D9_VCA_VT))
#define D9_SNAPPY_MIX 1.95f
#define D9_BD_BASE_HZ 49.0f
#define D9_BD_DF_PER_MS 4.6f
#define D9_BD_PHOLD_MS 16.0f
#define D9_INV_TANH_1_6 1.0849887132644653f    /* 1 / tanhf(1.6f) */
#define D9_INV_TANH_2_0 1.037314772605896f     /* 1 / tanhf(2.0f) */
#define D9_PH_QUARTER 0x40000000u              /* 9W9 starts every hit at phase 0.25 */

/* 9W9's back-to-back diode rounding at the two fixed drives the circuits use */
static inline float d9_diode16(float x) { return d9_tanh(1.6f * x) * D9_INV_TANH_1_6; }
static inline float d9_diode20(float x) { return d9_tanh(2.0f * x) * D9_INV_TANH_2_0; }

/* ===================================================================== */
/* Parameters                                                             */
/* ===================================================================== */
enum { CV_LIN, CV_EXP, CV_SW };
enum { F_TUNE, F_ATTACK, F_DECAY, F_LEVEL, F_PDEPTH, F_PITCH, F_DRIVE, F_DIST, F_REV, F_DLY,
       F_TONE, F_SNAPPY, F_TAIL, F_ACCENT, F_VEL, F_PAN };

typedef struct {
    x0x_param_t ui;
    uint8_t field, curve, exp;
    float lo, hi;
} d9_pspec_t;

static const char *const d9_dist_names[7] = { "Diode", "Clip", "SAT", "BFZ", "PDIST", "Fold", "Crush" };

#define LIN(nm, f, d, lo, hi) { { nm, 127, d, 0 }, f, CV_LIN, 0, lo, hi }
#define EXP(nm, f, d, tab)    { { nm, 127, d, 0 }, f, CV_EXP, X0X_EXP_##tab, 0.0f, 0.0f }
#define DRIVE(d)              EXP("Drive", F_DRIVE, d, DRIVE)
#define DIST                  { { "Dist", 6, 0, d9_dist_names }, F_DIST, CV_SW, 0, 0.0f, 0.0f }
#define SENDS                 LIN("Rev", F_REV, 0, 0.0f, 1.0f), LIN("Dly", F_DLY, 0, 0.0f, 1.0f)
#define LEVEL(d)              LIN("Level", F_LEVEL, d, 0.0f, 1.35f)
#define PAN                   { { "Pan", 127, 64, 0 }, F_PAN, CV_SW, 0, 0.0f, 0.0f }   /* X0X: last on every voice */

/* Pot defaults are the positions 9W9 seeds from its engineering defaults
 * (er99_engine_seed_pots); the engine itself starts on the exact defaults. */
static const d9_pspec_t d9_bd_p[] = {
    LIN("Tune", F_TUNE, 34, 6.0f, 32.0f), LIN("Attack", F_ATTACK, 13, 0.0f, 1.0f),
    EXP("Decay", F_DECAY, 90, BD_DECAY), LEVEL(94),
    LIN("P.Dpth", F_PDEPTH, 0, 0.0f, 1.0f), EXP("Pitch", F_PITCH, 45, BD_PITCH), DRIVE(0), DIST, SENDS, PAN,
};
static const d9_pspec_t d9_sd_p[] = {
    EXP("Tune", F_TUNE, 64, SD_TUNE), EXP("Tone", F_TONE, 68, SD_TONE),
    LIN("Snappy", F_SNAPPY, 64, 0.0f, 1.0f), LIN("Decay", F_DECAY, 41, 120.0f, 800.0f),   /* X0X: 9W9's fixed 340 ms */
    LEVEL(58), DRIVE(36), DIST, SENDS, PAN,
};
static const d9_pspec_t d9_lt_p[] = {
    EXP("Tune", F_TUNE, 66, LT_TUNE), EXP("Decay", F_DECAY, 111, LT_DECAY), LEVEL(52),
    LIN("Attack", F_ATTACK, 23, 0.0f, 1.0f), DRIVE(41), DIST, SENDS, PAN,
};
static const d9_pspec_t d9_mt_p[] = {
    EXP("Tune", F_TUNE, 69, MT_TUNE), EXP("Decay", F_DECAY, 100, MT_DECAY), LEVEL(52),
    LIN("Attack", F_ATTACK, 23, 0.0f, 1.0f), DRIVE(41), DIST, SENDS, PAN,
};
static const d9_pspec_t d9_ht_p[] = {
    EXP("Tune", F_TUNE, 53, HT_TUNE), EXP("Decay", F_DECAY, 105, HT_DECAY), LEVEL(52),
    LIN("Attack", F_ATTACK, 23, 0.0f, 1.0f), DRIVE(41), DIST, SENDS, PAN,
};
static const d9_pspec_t d9_rs_p[] = {
    LEVEL(103), EXP("Tune", F_TUNE, 62, RS_TUNE), DRIVE(24), DIST, SENDS, PAN,
};
static const d9_pspec_t d9_cp_p[] = {
    LEVEL(122), EXP("Tune", F_TUNE, 63, CP_TUNE), EXP("Tail", F_TAIL, 63, CP_TAIL), DRIVE(30), DIST, SENDS, PAN,
};
static const d9_pspec_t d9_ch_p[] = {
    EXP("Decay", F_DECAY, 84, CH_DECAY), LEVEL(89), EXP("Tune", F_TUNE, 64, SMP_PITCH), DRIVE(0), DIST, SENDS, PAN,
};
static const d9_pspec_t d9_oh_p[] = {
    EXP("Decay", F_DECAY, 97, OH_DECAY), LEVEL(80), EXP("Tune", F_TUNE, 64, SMP_PITCH), DRIVE(0), DIST, SENDS, PAN,
};
static const d9_pspec_t d9_cr_p[] = {
    EXP("Tune", F_TUNE, 64, SMP_PITCH), LEVEL(47), EXP("Decay", F_DECAY, 108, CY_DECAY), DRIVE(0), DIST, SENDS, PAN,
};
static const d9_pspec_t d9_rd_p[] = {
    EXP("Tune", F_TUNE, 64, SMP_PITCH), LEVEL(42), EXP("Decay", F_DECAY, 108, CY_DECAY), DRIVE(0), DIST, SENDS, PAN,
};
static const d9_pspec_t d9_kit_p[] = {
    LIN("Accent", F_ACCENT, 42, 1.0f, 4.0f), LIN("Veloc", F_VEL, 127, 0.0f, 1.0f),
};

#define NP(a) ((int)(sizeof(a) / sizeof(a[0])))
static const d9_pspec_t *const d9_specs[DR_NUM + 1] = {
    d9_bd_p, d9_sd_p, d9_lt_p, d9_mt_p, d9_ht_p, d9_rs_p, d9_cp_p, d9_ch_p, d9_oh_p, d9_cr_p, d9_rd_p, d9_kit_p,
};
/* every list must fit pots[][DR_MAX_PARAMS]: a longer one writes into the next voice's pots */
#define FITS(a) _Static_assert(NP(a) <= DR_MAX_PARAMS, #a " has more params than DR_MAX_PARAMS")
FITS(d9_bd_p); FITS(d9_sd_p); FITS(d9_lt_p); FITS(d9_mt_p); FITS(d9_ht_p); FITS(d9_rs_p); FITS(d9_cp_p);
FITS(d9_ch_p); FITS(d9_oh_p); FITS(d9_cr_p); FITS(d9_rd_p); FITS(d9_kit_p);
static const uint8_t d9_nspec[DR_NUM + 1] = {
    NP(d9_bd_p), NP(d9_sd_p), NP(d9_lt_p), NP(d9_mt_p), NP(d9_ht_p), NP(d9_rs_p), NP(d9_cp_p),
    NP(d9_ch_p), NP(d9_oh_p), NP(d9_cr_p), NP(d9_rd_p), NP(d9_kit_p),
};

int drum909_nparams(int voice)
{
    return voice >= 0 && voice <= DR_KIT ? d9_nspec[voice] : 0;
}

const x0x_param_t *drum909_param(int voice, int i)
{
    if (voice < 0 || voice > DR_KIT || i < 0 || i >= d9_nspec[voice])
        return 0;
    return &d9_specs[voice][i].ui;
}

int drum909_get(const drum909_t *d, int voice, int i)
{
    if (voice < 0 || voice > DR_KIT || i < 0 || i >= d9_nspec[voice])
        return 0;
    return d->pots[voice][i];
}

/* er99_pot_to_value */
static float d9_pot_value(const d9_pspec_t *s, int pot)
{
    if (s->curve == CV_EXP)
        return x0x_pot_exp[s->exp][pot];
    if (s->curve == CV_SW)
        return (float)pot;
    const float t = (float)pot / 127.0f;
    return s->lo + (s->hi - s->lo) * t;
}

/* ===================================================================== */
/* Rim and clap filter setup (er99_rim909_retune, er99_clap909_retune)    */
/* ===================================================================== */
static void d9_rim_retune(d9_rim_t *v)
{
    d9_biquad_set(&v->bp1, D9_BP, v->tune, v->res);
    d9_biquad_set(&v->bp2, D9_BP, v->tune2, v->res * 1.1f);
}

static void d9_clap_retune(d9_clap_t *v) { d9_biquad_set(&v->bp, D9_BP, v->tune, v->res); }

static void d9_smp_rate(d9_smp_t *s)
{
    const float p = s->pitch > 0.0f ? s->pitch : 1.0f;
    s->inc = (uint32_t)p;
    s->incf = (uint32_t)((p - (float)s->inc) * 4294967296.0f);
}

/* ===================================================================== */
/* Apply one parameter (er99_engine_set_raw for the keys the panel has)   */
/* ===================================================================== */
static void d9_apply(drum909_t *d, int voice, int field, float v)
{
    if (field == F_PAN) {
        if (voice < DR_NUM)
            x0x_pan_gains((int)v, &d->pan_l[voice], &d->pan_r[voice]);
        return;
    }
    if (field == F_REV || field == F_DLY) {
        float *a = field == F_DLY ? d->send_dly : d->send_rev;
        a[voice] = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        return;
    }
    if (voice == DR_KIT) {
        if (field == F_ACCENT) d->accent = v;
        else d->vel_depth = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        return;
    }
    if (voice <= DR_HT) {
        d9_bt_t *b = &d->bt[voice];
        switch (field) {
        case F_TUNE: b->tune = v; break;
        case F_ATTACK: b->attack = v; break;
        case F_DECAY: b->decay = v; break;
        case F_LEVEL: b->level = v; break;
        case F_PDEPTH: b->sweep_depth = v; break;
        case F_PITCH: b->pitch_mod = v; break;
        case F_TONE: b->noise_decay = v; break;
        case F_SNAPPY: b->snappy = v; break;
        case F_DRIVE: b->drive = v; break;
        case F_DIST: b->dist_type = (int32_t)v; break;
        default: break;
        }
        d9_shape_prep(&b->shape, b->drive, b->dist_type);
        return;
    }
    if (voice == DR_RS) {
        d9_rim_t *r = &d->rim;
        switch (field) {
        case F_TUNE: r->tune = v; d9_rim_retune(r); break;
        case F_LEVEL: r->level = v; break;
        case F_DRIVE: r->drive = v; break;
        case F_DIST: r->dist_type = (int32_t)v; break;
        default: break;
        }
        d9_shape_prep(&r->shape, r->drive, r->dist_type);
        return;
    }
    if (voice == DR_CP) {
        d9_clap_t *c = &d->clap;
        switch (field) {
        case F_TUNE: c->tune = v; d9_clap_retune(c); break;
        case F_TAIL: c->tail_decay = v; break;
        case F_LEVEL: c->level = v; break;
        case F_DRIVE: c->drive = v; break;
        case F_DIST: c->dist_type = (int32_t)v; break;
        default: break;
        }
        d9_shape_prep(&c->shape, c->drive, c->dist_type);
        return;
    }
    d9_smp_t *s = &d->smp[voice - DR_CH];
    switch (field) {
    case F_TUNE: s->pitch = v; d9_smp_rate(s); break;
    case F_DECAY: s->decay = v; break;
    case F_LEVEL: s->volume = v; break;
    case F_DRIVE: s->drive = v; break;
    case F_DIST: s->dist_type = (int32_t)v; break;
    default: break;
    }
    d9_shape_prep(&s->shape, s->drive, s->dist_type);
}

void drum909_set(drum909_t *d, int voice, int i, int value)
{
    if (voice < 0 || voice > DR_KIT || i < 0 || i >= d9_nspec[voice])
        return;
    const d9_pspec_t *s = &d9_specs[voice][i];
    if (value < 0) value = 0;
    if (value > s->ui.max) value = s->ui.max;
    d->pots[voice][i] = (uint8_t)value;
    d9_apply(d, voice, s->field, d9_pot_value(s, value));
}

/* ===================================================================== */
/* Init (er99_engine_init)                                                */
/* ===================================================================== */
static void d9_bt_init(d9_bt_t *v)
{
    d9_biquad_set(&v->dc_block, D9_HP, 20.0f, 0.7071f);
    d9_env_set(&v->pitch, v->tune);
    d9_env_set(&v->amp, 0.0f);
    d9_env_set(&v->click_env, 0.0f);
    d9_env_set(&v->noise_env, 0.0f);
    d9_biquad_set(&v->click_lp, D9_LP, v->click_tone > 0.0f ? v->click_tone : 3000.0f, 0.7071f);
    d9_biquad_set(&v->noise_hpf, D9_HP, v->noise_hp > 0.0f ? v->noise_hp : 800.0f, 0.7071f);
    d9_biquad_set(&v->noise_lpf, D9_LP, 6500.0f, 0.7071f);
    if (v->pitch_mod <= 0.0f)
        v->pitch_mod = 1.0f;
    d9_shape_prep(&v->shape, v->drive, v->dist_type);
}

void drum909_init(drum909_t *d)
{
    uint8_t *p = (uint8_t *)d;
    for (unsigned i = 0; i < sizeof(*d); ++i)
        p[i] = 0;
    for (int v = 0; v < DR_NUM; ++v)
        d->pan_l[v] = d->pan_r[v] = d->pan_lc[v] = d->pan_rc[v] = 1.0f;   /* centre */
    d->noise = 0xC0FFEEu;
    d->nz_pos = DR_NZ_HIST;

    for (int i = 0; i < 5; ++i) {
        d9_bt_t *b = &d->bt[i];
        b->drive = 2.0f; b->level = 0.8f; b->click_tone = 3000.0f;
        switch (i) {
        case 0:
            b->tune = 13.0f; b->sweep_depth = 0.0f; b->pitch_mod = 1.0f;
            b->decay = 1380.0f; b->amp_hold = 40.0f;
            b->attack = 0.10f; b->click_tone = 2500.0f;
            b->drive = 0.2f; b->level = 1.0f;
            break;
        case 1:
            b->tune = 205.0f; b->tune2 = 325.0f; b->osc2_mix = 0.40f;
            b->sweep_depth = 1.045f; b->sweep_time = 30.0f;
            b->decay = 340.0f; b->attack = 0.15f;
            b->snappy = 0.5f; b->noise_decay = 1200.0f; b->noise_hp = 1000.0f;
            b->drive = 1.8f; b->level = 0.62f;
            break;
        case 2:
            b->tune = 68.0f; b->sweep_depth = 1.12f; b->sweep_time = 200.0f;
            b->decay = 1700.0f; b->attack = 0.18f; b->level = 0.55f;
            break;
        case 3:
            b->tune = 102.0f; b->sweep_depth = 1.12f; b->sweep_time = 180.0f;
            b->decay = 1050.0f; b->attack = 0.18f; b->level = 0.55f;
            break;
        default:
            b->tune = 132.0f; b->sweep_depth = 1.12f; b->sweep_time = 160.0f;
            b->decay = 1100.0f; b->attack = 0.18f; b->level = 0.55f;
            break;
        }
        d9_bt_init(b);
    }
    for (int i = 0; i < 3; ++i) {
        d9_tom_t *t = &d->tom[i];
        for (int j = 0; j < 3; ++j)
            d9_env_set(&t->env[j], 0.0f);
        d9_env_set(&t->pitch, 100.0f);
        d9_env_set(&t->noise_env, 0.0f);
        d9_biquad_set(&t->noise_bp, D9_BP, 1200.0f, 1.2f);
        d9_biquad_set(&t->dc_block, D9_HP, 20.0f, 0.7071f);
    }

    d9_rim_t *r = &d->rim;
    r->tune = 210.0f; r->tune2 = 480.0f; r->res = 10.0f;
    r->decay = 200.0f; r->noise_mix = 0.35f;
    r->drive = 1.4f; r->dist_type = 0; r->level = 1.1f;
    d9_biquad_set(&r->bp1, D9_BP, r->tune, r->res);
    d9_biquad_set(&r->bp2, D9_BP, r->tune2, r->res * 1.1f);
    d9_biquad_set(&r->hp, D9_HP, 120.0f, 0.7071f);
    d9_env_set(&r->amp, 0.0f);
    r->accent = 1.0f;
    d9_shape_prep(&r->shape, r->drive, r->dist_type);

    d9_clap_t *c = &d->clap;
    c->tune = 950.0f; c->res = 2.0f;
    c->spread = 12.0f; c->burst_decay = 50.0f;
    c->tail_decay = 345.0f; c->tail_level = 0.58f;
    c->drive = 1.6f; c->dist_type = 0; c->level = 1.3f;
    d9_biquad_set(&c->bp, D9_BP, c->tune, c->res);
    d9_biquad_set(&c->hp, D9_HP, 400.0f, 0.7071f);
    d9_env_set(&c->burst, 0.0f);
    d9_env_set(&c->tail, 0.0f);
    c->pulse_index = 4;
    c->accent = 1.0f;
    d9_shape_prep(&c->shape, c->drive, c->dist_type);

    /* CH OH CR RD (9W9 orders them ohh rc cr chh) */
    static const float vols[4] = { 0.95f, 0.85f, 0.5f, 0.45f };
    static const float decs[4] = { 110.0f, 450.0f, 1800.0f, 1800.0f };
    for (int i = 0; i < 4; ++i) {
        d9_smp_t *s = &d->smp[i];
        s->volume = vols[i];
        s->decay = decs[i];
        s->drive = 0.2f;
        s->dist_type = 0;
        s->pitch = 1.0f;
        d9_smp_rate(s);
        d9_env_set(&s->out, 0.0f);
        d9_shape_prep(&s->shape, s->drive, s->dist_type);
        if (i == 2) { s->buf = x0x_smp_crash; s->len = X0X_SMP_CRASH_LEN; }
        else if (i == 3) { s->buf = x0x_smp_ride; s->len = X0X_SMP_RIDE_LEN; }
        else { s->buf = x0x_smp_hh; s->len = X0X_SMP_HH_LEN; }
    }

    d->accent = 2.0f;
    d->vel_depth = 1.0f;

    for (int v = 0; v <= DR_KIT; ++v)
        for (int i = 0; i < d9_nspec[v]; ++i)
            d->pots[v][i] = d9_specs[v][i].ui.def;
}

/* ===================================================================== */
/* Triggers (er99_engine_trigger and the voices' *_trigger)                */
/* ===================================================================== */
static void d9_bt_trigger(d9_bt_t *v, int bd, float accent)
{
    if (v->click_stale && v->mute > 0)
        v->click_warm = 1;
    v->click_stale = 0;
    v->ph = v->ph2 = D9_PH_QUARTER;
    d9_biquad_reset(&v->dc_block);
    if (bd) {
        const float tau = v->tune < 6.0f ? 6.0f : (v->tune > 32.0f ? 32.0f : v->tune);
        const float m = v->sweep_depth < 0.0f ? 0.0f : (v->sweep_depth > 1.0f ? 1.0f : v->sweep_depth);
        const float pm = v->pitch_mod > 0.0f ? v->pitch_mod : 1.0f;
        v->bd_base = D9_BD_BASE_HZ * (1.0f + m * (pm - 1.0f));
        v->bd_df = D9_BD_DF_PER_MS * tau * (1.0f + 1.2f * m);
        v->bd_mult = d9_exp_small(-1.0f / (tau * D9_MS));
        v->bd_phold = (int32_t)(D9_BD_PHOLD_MS * D9_MS);
        d9_env_set(&v->pitch, v->bd_base);
    } else {
        d9_env_set(&v->pitch, v->tune * v->sweep_depth);
        d9_env_exp(&v->pitch, v->tune, v->sweep_time * D9_MS);
    }
    d9_env_set(&v->amp, 1.0f);
    v->amp_hold_left = (int32_t)(v->amp_hold * D9_MS);
    if (v->amp_hold_left <= 0)
        d9_env_exp(&v->amp, 0.00001f, v->decay * D9_MS);
    d9_env_set(&v->click_env, 1.0f);
    d9_env_exp(&v->click_env, 0.00001f, 3.0f * D9_MS);
    if (v->snappy > 0.0f) {
        d9_env_set(&v->noise_env, 1.0f);
        v->noise_hold = (int32_t)(24.0f * D9_MS);
        v->noise_gated = 0;
    }
    v->impulse = 2;
    v->out_gain = v->level * accent;
    const float longest = v->decay > v->noise_decay ? v->decay : v->noise_decay;
    v->mute = d9_ceil((longest + 20.0f) * D9_MS);
}

static void d9_tom_trigger(d9_tom_t *t, const d9_bt_t *p, float accent)
{
    if (t->stick_stale && t->mute > 0)
        t->stick_warm = 1;
    t->stick_stale = 0;
    for (int i = 0; i < 3; ++i)
        t->ph[i] = D9_PH_QUARTER;
    d9_biquad_reset(&t->dc_block);
    d9_env_set(&t->pitch, p->tune * p->sweep_depth);
    d9_env_exp(&t->pitch, p->tune, p->sweep_time * D9_MS);
    const float d1 = p->decay;
    d9_env_set(&t->env[0], 1.0f);
    d9_env_exp(&t->env[0], 0.00001f, d1 * D9_MS);
    d9_env_set(&t->env[1], 0.55f);
    d9_env_exp(&t->env[1], 0.00001f, 110.0f * D9_MS);
    d9_env_set(&t->env[2], 0.45f);
    d9_env_exp(&t->env[2], 0.00001f, 70.0f * D9_MS);
    t->noise_level = p->attack;
    d9_env_set(&t->noise_env, 1.0f);
    d9_env_exp(&t->noise_env, 0.00001f, 8.0f * D9_MS);
    t->out_gain = p->level * accent;
    t->mute = d9_ceil((d1 + 60.0f) * D9_MS);
}

void drum909_trigger(drum909_t *d, int voice, float vel)
{
    if (voice < 0 || voice >= DR_NUM)
        return;
    if (!(vel >= 0.0f)) vel = 0.0f;
    if (vel > 1.0f) vel = 1.0f;
    const float vgain = d->accent * (1.0f - d->vel_depth * (1.0f - vel));

    switch (voice) {
    case DR_BD: {
        d9_bt_t *b = &d->bt[DR_BD];
        b->tune2 = 0.0f; b->osc2_mix = 0.0f;
        b->click_tone = 2500.0f;
        b->amp_hold = 40.0f;
        d9_bt_trigger(b, 1, vgain);
        break;
    }
    case DR_SD: {
        d9_bt_t *b = &d->bt[DR_SD];
        b->tune2 = b->tune * 1.585f;
        b->osc2_mix = 0.40f;
        b->sweep_depth = 1.045f;
        b->sweep_time = 30.0f;
        b->attack = 0.0f;                     /* (X0X: the body's decay is its DECAY pot, 9W9's 340 ms by default) */
        if (b->noise_hp < 990.0f || b->noise_hp > 1010.0f) {
            b->noise_hp = 1000.0f;
            d9_biquad_set(&b->noise_hpf, D9_HP, 1000.0f, 0.7071f);
        }
        d9_bt_trigger(b, 0, vgain);
        break;
    }
    case DR_LT: case DR_MT: case DR_HT:
        d9_tom_trigger(&d->tom[voice - DR_LT], &d->bt[voice], vgain);
        break;
    case DR_RS: {
        d9_rim_t *r = &d->rim;
        const float t2 = r->tune * 2.286f, q = 10.0f;
        if (r->tune2 != t2 || r->res != q) {
            r->tune2 = t2; r->res = q;
            d9_rim_retune(r);
        }
        r->noise_mix = 0.35f;
        r->decay = 200.0f;
        d9_env_set(&r->amp, 1.0f);
        d9_env_exp(&r->amp, 0.00001f, r->decay * D9_MS);
        r->impulse = 2;
        r->accent = vgain;
        r->mute = d9_ceil((r->decay + 20.0f) * D9_MS);
        break;
    }
    case DR_CP: {
        d9_clap_t *c = &d->clap;
        c->pulse_index = 0;
        c->next_pulse = 0.0f;
        d9_env_set(&c->tail, 0.0f);
        c->accent = vgain;
        c->mute = d9_ceil((c->tail_decay + c->spread * 4.0f + 40.0f) * D9_MS);
        break;
    }
    default: {
        d9_smp_t *s = &d->smp[voice - DR_CH];
        if (voice == DR_CH || voice == DR_OH) {
            /* one pair of cymbals: each choke the other with a 3 ms fade */
            d9_smp_t *o = &d->smp[voice == DR_CH ? DR_OH - DR_CH : 0];
            d9_env_exp(&o->out, 0.00001f, 3.0f * 0.001f * D9_SR);
            o->mute = d9_ceil(6.0f * 0.001f * D9_SR);
        }
        d9_env_set(&s->out, s->volume * vgain);
        d9_env_exp(&s->out, 0.00001f, s->decay * 0.001f * D9_SR);
        s->pos = 0;
        s->frac = 0;
        s->playing = 1;
        s->mute = d9_ceil(s->decay * 0.001f * D9_SR);
        break;
    }
    }
}

/* ===================================================================== */
/* Rendering                                                              */
/* ===================================================================== */
typedef struct {
    float *dry, *rev, *dly;   /* rev / dly are 0 when the voice's send is 0 */
    float srev, sdly;
    float *dry_r, pl, pr;     /* X0X stereo: dry_r 0 = mono (dry gets y as 9W9's) */
    float dpl, dpr;           /* the pans' glide per sample this block (0 settled) */
} d9_bus_t;

static inline void d9_emit(const d9_bus_t *b, int i, float y)
{
    if (b->dry_r) {
        b->dry[i] += y * (b->pl + b->dpl * (float)i);
        b->dry_r[i] += y * (b->pr + b->dpr * (float)i);
    } else
        b->dry[i] += y;
    if (b->rev) b->rev[i] += y * b->srev;
    if (b->dly) b->dly[i] += y * b->sdly;
}

/* A filter on the shared noise that a voice stopped running (its envelope had
 * run out) is brought back to the state 9W9's would have, by running it over
 * the noise it missed. Both filters forget fast (the click LP's poles are at
 * r = 0.78, the stick BP's at 0.93: r^DR_NZ_HIST < 2e-8), so the last
 * DR_NZ_HIST samples are all that matter. `end` points one past the last
 * sample the filter should have seen; the DR_NZ_HIST before it are readable. */
static void d9_warm(d9_biquad_t *f, const float *end)
{
    d9_biquad_reset(f);
    for (int i = -DR_NZ_HIST; i < 0; ++i)
        d9_biquad_tick(f, end[i]);
}

/* Kick (er99_bt_render with the stock sweep, one shell) */
static void d9_render_bd(d9_bt_t *v, const float *nz, const d9_bus_t *bus, int n)
{
    const int m = n < v->mute ? n : v->mute;
    v->mute -= m;
    d9_env_anchor(&v->amp);
    d9_env_anchor(&v->click_env);
    const float attack = v->attack;
    if (v->click_warm) {                 /* retriggered while its click filter was skipped */
        d9_warm(&v->click_lp, nz);
        v->click_warm = 0;
    }
    for (int i = 0; i < m; ++i) {
        const float f = v->bd_base + v->bd_df;
        if (v->bd_phold > 0)
            --v->bd_phold;
        else
            v->bd_df *= v->bd_mult;
        if (v->amp_hold_left > 0 && --v->amp_hold_left == 0)
            d9_env_exp(&v->amp, 0.00001f, v->decay * 0.001f * D9_SR);
        const float amp = d9_env_tick(&v->amp);

        v->ph += d9_inc(f);
        float o = d9_diode16(d9_tri(v->ph));
        o += 0.05f * o * o;
        o = d9_biquad_tick(&v->dc_block, o);
        const float body = d9_shape(&v->shape, o, v->crush_st) * amp;

        /* the beater click; once its 3 ms envelope has run out it sits at -100 dB
         * for the rest of the note, and is skipped */
        float click = 0.0f;
        if (v->impulse > 0 || v->click_env.left) {
            if (v->impulse > 0) { click += 1.0f; v->impulse--; }
            click += d9_biquad_tick(&v->click_lp, nz[i]);
            click *= d9_env_tick(&v->click_env) * attack;
        }
        d9_emit(bus, i, (body + click) * v->out_gain);
    }
    if (!(v->impulse > 0 || v->click_env.left))
        v->click_stale = 1;
    if (v->click_stale && v->mute == 0) {  /* note over: freeze where 9W9's froze */
        d9_warm(&v->click_lp, nz + m);
        v->click_stale = 0;
    }
}

/* Snare (er99_bt_render, two shells + ENV4 noise). Its click path is pinned
 * off (attack 0 at every trigger), so it is not computed: 0 either way. */
static void d9_render_sd(d9_bt_t *v, const float *nz, const d9_bus_t *bus, int n)
{
    const int m = n < v->mute ? n : v->mute;
    v->mute -= m;
    d9_env_anchor(&v->pitch);
    d9_env_anchor(&v->amp);
    d9_env_anchor(&v->noise_env);
    const float ratio = v->tune > 1.0f ? v->tune2 / v->tune : 1.0f;
    const float mix2 = v->osc2_mix, inv_mix = 1.0f / (1.0f + v->osc2_mix);
    for (int i = 0; i < m; ++i) {
        const float f = d9_env_tick(&v->pitch);
        float amp = d9_env_tick(&v->amp);
        amp = amp > D9_VCA_VT ? (amp - D9_VCA_VT) * D9_VCA_NORM : 0.0f;

        v->ph += d9_inc(f);
        v->ph2 += d9_inc(f * ratio);
        float o = d9_diode20(d9_tri(v->ph));
        o += d9_diode20(d9_tri(v->ph2)) * mix2;
        o *= inv_mix;
        float y = d9_shape(&v->shape, o, v->crush_st) * amp;

        if (v->snappy > 0.0f && !v->noise_gated) {
            if (v->noise_hold > 0 && --v->noise_hold == 0)
                d9_env_exp(&v->noise_env, 0.00001f,
                           (v->noise_decay > 0.0f ? v->noise_decay : 120.0f) * 0.7f * 0.001f * D9_SR);
            float env = d9_env_tick(&v->noise_env);
            if (v->noise_hold <= 0) {
                env = env > D9_VCA_VT ? (env - D9_VCA_VT) * D9_VCA_NORM : 0.0f;
                if (env <= 0.0f)
                    v->noise_gated = 1;
            }
            y += d9_biquad_tick(&v->noise_lpf, d9_biquad_tick(&v->noise_hpf, nz[i])) * env * v->snappy
               * D9_SNAPPY_MIX;
        }
        d9_emit(bus, i, y * v->out_gain);
    }
}

/* Toms (er99_tom909_render): three VCOs on their own envelopes + stick noise.
 * The two upper partials and the stick are attack features (110 / 70 / 8 ms);
 * once their envelopes have run out (-100 dB) they are skipped. */
static void d9_render_tom(d9_tom_t *t, const d9_bt_t *p, const float *nz, const d9_bus_t *bus, int n)
{
    const int m = n < t->mute ? n : t->mute;
    t->mute -= m;
    d9_env_anchor(&t->pitch);
    d9_env_anchor(&t->env[0]);
    d9_env_anchor(&t->env[1]);
    d9_env_anchor(&t->env[2]);
    d9_env_anchor(&t->noise_env);
    const float stick_g = t->noise_level * 1.5f;
    if (t->stick_warm) {
        d9_warm(&t->noise_bp, nz);
        t->stick_warm = 0;
    }
    for (int i = 0; i < m; ++i) {
        const float f = d9_env_tick(&t->pitch);
        float o = 0.0f;
        t->ph[0] += d9_inc(f);
        o += d9_diode20(d9_tri(t->ph[0])) * d9_env_tick(&t->env[0]);
        if (t->env[1].left) {
            t->ph[1] += d9_inc(f * 1.50f);
            o += d9_diode20(d9_tri(t->ph[1])) * d9_env_tick(&t->env[1]);
        }
        if (t->env[2].left) {
            t->ph[2] += d9_inc(f * 2.75f);
            o += d9_diode20(d9_tri(t->ph[2])) * d9_env_tick(&t->env[2]);
        }
        o *= 0.55f;
        float stick = 0.0f;
        if (t->noise_env.left)
            stick = d9_biquad_tick(&t->noise_bp, nz[i]) * d9_env_tick(&t->noise_env) * stick_g;
        float y = d9_shape(&p->shape, o + stick, t->crush_st);
        y = d9_biquad_tick(&t->dc_block, y);
        d9_emit(bus, i, y * t->out_gain);
    }
    if (!t->noise_env.left)
        t->stick_stale = 1;
    if (t->stick_stale && t->mute == 0) {
        d9_warm(&t->noise_bp, nz + m);
        t->stick_stale = 0;
    }
}

/* Rim shot (er99_rim909_render) */
static void d9_render_rim(d9_rim_t *v, const float *nz, const d9_bus_t *bus, int n)
{
    const int m = n < v->mute ? n : v->mute;
    v->mute -= m;
    d9_env_anchor(&v->amp);
    const float g = v->level;
    for (int i = 0; i < m; ++i) {
        float exc = 0.0f;
        if (v->impulse > 0) {
            exc += 1.0f + nz[i] * v->noise_mix;
            v->impulse--;
        }
        const float a = d9_env_tick(&v->amp);
        float y = d9_biquad_tick(&v->bp1, exc) * 60.0f + d9_biquad_tick(&v->bp2, exc) * 32.0f;
        y = d9_shape(&v->shape, y * a, v->crush_st);
        y = d9_biquad_tick(&v->hp, y);
        d9_emit(bus, i, y * g * v->accent);
    }
}

/* Hand clap (er99_clap909_render) */
static void d9_render_clap(d9_clap_t *v, const float *nz, const d9_bus_t *bus, int n)
{
    static const float amp[4] = { 1.22f, 1.35f, 1.68f, 1.0f };
    const int m = n < v->mute ? n : v->mute;
    v->mute -= m;
    d9_env_anchor(&v->burst);
    d9_env_anchor(&v->tail);
    for (int i = 0; i < m; ++i) {
        if (v->pulse_index < 4) {
            if (v->next_pulse <= 0.0f) {
                d9_env_set(&v->burst, amp[v->pulse_index]);
                d9_env_exp(&v->burst, 0.00001f, v->burst_decay * D9_MS);
                if (v->pulse_index == 3) {
                    d9_env_set(&v->tail, v->tail_level);
                    d9_env_exp(&v->tail, 0.00001f, v->tail_decay * D9_MS);
                }
                v->pulse_index++;
                v->next_pulse = v->spread * D9_MS;
            }
            v->next_pulse -= 1.0f;
        }
        const float env = d9_env_tick(&v->burst) + d9_env_tick(&v->tail);
        float y = d9_biquad_tick(&v->bp, nz[i]) * env;
        y = d9_shape(&v->shape, y, v->crush_st);
        y = d9_biquad_tick(&v->hp, y);
        d9_emit(bus, i, y * v->level * v->accent);
    }
}

/* Hats, ride, crash (render_sampler). Past the sample's end and at a whole-number rate: after Optimist
 * (github.com/w0ts/optimist 537fbf00, GPL-3.0-only), the same samples */
static void d9_render_smp(d9_smp_t *s, const d9_bus_t *bus, int n)
{
    if (!s->playing)
        return;
    const int m = n < s->mute ? n : s->mute;
    s->mute -= m;
    const int drive = s->drive > 0.25f;
    const uint32_t len = s->len;
    /* after Optimist: past the sample's end every sample is 0 x the envelope (+0: the envelope is positive), and
     * the drive stage's +0 for the shapes that give +0 at 0 (Diode, SAT, PDIST, Fold; not the asymmetric clip or
     * BFZ, whose small DC at 0 stays, nor Crush, whose hold count runs on): nothing to add. The envelope is not
     * needed again (a hit sets it anew; a choke's ramp only scales these 0s) */
    if (s->pos >= len && (!drive || s->shape.type == 0 || s->shape.type == 2 || s->shape.type == 4
                          || s->shape.type == 5))
        return;
    d9_env_anchor(&s->out);
    const float pre = 1.0f + s->drive * 0.5f;
    const uint8_t *buf = s->buf;
    if (s->incf == 0u && s->frac == 0u && !drive) {
        /* after Optimist: at a whole-number rate (Tune at its centre: 1.0) the read stays on the samples: fr is 0
         * and a + (b - a) 0 is a */
        const uint32_t inc = s->inc;
        uint32_t p = s->pos;
        for (int i = 0; i < m; ++i) {
            float v = 0.0f;
            if (p < len) {
                v = (float)x0x_mulaw[buf[p]] * (1.0f / 32768.0f);
                p += inc;
            }
            v *= d9_env_tick(&s->out);
            d9_emit(bus, i, v);
        }
        s->pos = p;
        return;
    }
    for (int i = 0; i < m; ++i) {
        float v = 0.0f;
        if (s->pos < len) {
            const float a = (float)x0x_mulaw[buf[s->pos]] * (1.0f / 32768.0f);
            const float b = s->pos + 1 < len ? (float)x0x_mulaw[buf[s->pos + 1]] * (1.0f / 32768.0f) : 0.0f;
            const float fr = (float)s->frac * (1.0f / 4294967296.0f);
            v = a + (b - a) * fr;
            const uint32_t f2 = s->frac + s->incf;
            s->pos += s->inc + (f2 < s->frac);
            s->frac = f2;
        }
        v *= d9_env_tick(&s->out);
        if (drive)
            v = d9_shape(&s->shape, v * pre, s->crush_st);
        d9_emit(bus, i, v);
    }
}

static float *d9_dry_r;         /* the render in progress: its right side (0: mono) */
static float d9_pl0[DR_NUM], d9_pr0[DR_NUM];   /* each voice's pan where this block starts */
static inline void d9_bus(d9_bus_t *b, drum909_t *d, int voice, float *dry, float *rev, float *dly)
{
    b->dry = dry;
    b->dry_r = d9_dry_r;
    b->pl = d9_pl0[voice];                         /* the pan glides from here over this block */
    b->pr = d9_pr0[voice];
    b->dpl = d->pan_dl[voice];
    b->dpr = d->pan_dr[voice];
    b->srev = d->send_rev[voice];
    b->sdly = d->send_dly[voice];
    b->rev = b->srev != 0.0f ? rev : 0;
    b->dly = b->sdly != 0.0f ? dly : 0;
}

static void d9_render(drum909_t *d, float *dry, float *rev, float *dly, int n)
{
    d9_bus_t bus;
    if (n > 256)
        n = 256;
    /* the shared noise, written on after the previous block so nz[-DR_NZ_HIST
     * .. -1] is the noise before this block; the tail is moved to the front
     * only when the buffer runs out (every other 256-frame block) */
    if (d->nz_pos + n > DR_NZ_BUF) {
        for (int i = 0; i < DR_NZ_HIST; ++i)
            d->nz_buf[i] = d->nz_buf[d->nz_pos - DR_NZ_HIST + i];
        d->nz_pos = DR_NZ_HIST;
    }
    float *const nz = d->nz_buf + d->nz_pos;
    d->nz_pos += n;
    for (int i = 0; i < n; ++i)
        nz[i] = d9_noise(&d->noise);

    /* 9W9's summing order: BD SD LT MT HT RS CP OH RD CR CH */
    if (d->bt[DR_BD].mute > 0) {
        d9_bus(&bus, d, DR_BD, dry, rev, dly);
        d9_render_bd(&d->bt[DR_BD], nz, &bus, n);
    }
    if (d->bt[DR_SD].mute > 0) {
        d9_bus(&bus, d, DR_SD, dry, rev, dly);
        d9_render_sd(&d->bt[DR_SD], nz, &bus, n);
    }
    for (int t = 0; t < 3; ++t)
        if (d->tom[t].mute > 0) {
            d9_bus(&bus, d, DR_LT + t, dry, rev, dly);
            d9_render_tom(&d->tom[t], &d->bt[DR_LT + t], nz, &bus, n);
        }
    if (d->rim.mute > 0) {
        d9_bus(&bus, d, DR_RS, dry, rev, dly);
        d9_render_rim(&d->rim, nz, &bus, n);
    }
    if (d->clap.mute > 0) {
        d9_bus(&bus, d, DR_CP, dry, rev, dly);
        d9_render_clap(&d->clap, nz, &bus, n);
    }
    static const uint8_t order[4] = { DR_OH, DR_RD, DR_CR, DR_CH };
    for (int k = 0; k < 4; ++k) {
        d9_smp_t *s = &d->smp[order[k] - DR_CH];
        if (s->mute > 0) {
            d9_bus(&bus, d, order[k], dry, rev, dly);
            d9_render_smp(s, &bus, n);
        }
    }
}

void drum909_render(drum909_t *d, float *dry, float *rev, float *dly, int n)
{
    d9_dry_r = 0;
    d9_render(d, dry, rev, dly, n);
}

void drum909_render_st(drum909_t *d, float *dry_l, float *dry_r, float *rev, float *dly, int n)
{
    d9_dry_r = dry_r;
    for (int v = 0; v < DR_NUM; ++v) {             /* every voice's pan glides, sounding or not */
        d9_pl0[v] = d->pan_lc[v];
        d9_pr0[v] = d->pan_rc[v];
        d->pan_dl[v] = x0x_glide_block(&d->pan_lc[v], d->pan_l[v], n > 256 ? 256 : n);
        d->pan_dr[v] = x0x_glide_block(&d->pan_rc[v], d->pan_r[v], n > 256 ? 256 : n);
    }
    d9_render(d, dry_l, rev, dly, n);
    d9_dry_r = 0;
}

void drum909_pan_settle(drum909_t *d)
{
    for (int v = 0; v < DR_NUM; ++v) {
        d->pan_lc[v] = d->pan_l[v];
        d->pan_rc[v] = d->pan_r[v];
    }
}

int drum909_active(const drum909_t *d)
{
    for (int i = 0; i < 5; ++i)
        if (i < 2 ? d->bt[i].mute > 0 : d->tom[i - 2].mute > 0)
            return 1;
    if (d->rim.mute > 0 || d->clap.mute > 0)
        return 1;
    for (int i = 0; i < 4; ++i)
        if (d->smp[i].mute > 0 && d->smp[i].playing)
            return 1;
    return 0;
}
