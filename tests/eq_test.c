/* (p9) the master's EQ (fx.c eq_block, FX > EQ: params.c eq_v) through the real sources (ui_test.c's):
 * 1. flat (all 0): bit for bit the input (the four bands sum back to it);
 * 2. each band +6 / -6 dB: a tone in it moves at least half of it, more than any other tone (broad one-pole bands,
 *    a Baxandall's gentleness: the neighbours follow a little);
 * 3. kept with a project ('E' 'Q'), one without it opens flat. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static int bad;
static void ck(const char *w, int ok) { printf("eq: %-96s %s\n", w, ok ? "ok" : "FAIL"); bad += !ok; }
static double level(double f, int band, int db)
{
    uint32_t b, i; double ph = 0, acc = 0; int k;
    memset(&eqs, 0, sizeof eqs);
    for (k = 0; k < 4; k++) eq_v[k] = 0;
    if (band >= 0) eq_v[band] = (int16_t)db;
    for (b = 0; b < 300u; b++) {
        for (i = 0; i < CTL; i++) { mix_l[i] = mix_r[i] = (int32_t)(40000 * sin(ph)); wet[i] = 0; ph += 2 * M_PI * f / FS; }
        eq_block(CTL);
        if (b >= 100u) for (i = 0; i < CTL; i++) acc += (double)mix_l[i] * mix_l[i];
    }
    return 10 * log10(acc / (200.0 * CTL) / (40000.0 * 40000.0 / 2));
}
int main(void)
{
    static const double F[4] = {60, 450, 2000, 9000};
    int band, ok, k;
    double l;
    {   uint32_t i, same = 1;
        memset(&eqs, 0, sizeof eqs); for (k = 0; k < 4; k++) eq_v[k] = 0;
        for (i = 0; i < CTL; i++) { mix_l[i] = mix_r[i] = (int32_t)(i * 997u % 60000u) - 30000; wet[i] = 0; }
        { int32_t ref[CTL]; for (i = 0; i < CTL; i++) ref[i] = mix_l[i]; eq_block(CTL); for (i = 0; i < CTL; i++) same &= mix_l[i] == ref[i]; }
        ck("1. flat: bit for bit the input", same);
    }
    for (band = 0; band < 4; band++) {
        int db;
        for (db = -6; db <= 6; db += 12) {
            double own = 0, other = 0;
            printf("eq:   band %d %+d dB:", band, db);
            for (k = 0; k < 4; k++) {
                l = level(F[k], band, db);
                printf(" %5.0f Hz %+5.1f", F[k], l);
                if (k == band) own = l; else if (fabs(l) > fabs(other)) other = l;
            }
            ok = fabs(own) >= fabs(db) / 2.0 && (own > 0) == (db > 0) && fabs(own) > fabs(other);
            printf("\n");
            ck(db > 0 ? "2. the band +6 dB: its tone up at least 3 dB, more than any other" : "   the band -6 dB the same", ok);
        }
    }
    {
        static project_store_t st;
        ui_power_on();
        eq_v[0] = 5; eq_v[1] = -3; eq_v[2] = 0; eq_v[3] = 12;
        project_capture(&proj_scratch);
        ok = proj_pack(&st, &proj_scratch);
        eq_v[0] = eq_v[1] = eq_v[2] = eq_v[3] = 0;
        ok = ok && proj_import(&proj_scratch, &st, sizeof st);
        ok = ok && project_restore_runtime(&proj_scratch) == 0;   /* (V10.58: set when the project is opened, not by the read) */
        ck("3. a project keeps the EQ (5 -3 0 12)", ok && eq_v[0] == 5 && eq_v[1] == -3 && eq_v[2] == 0 && eq_v[3] == 12);
        eq_v[0] = eq_v[1] = eq_v[2] = eq_v[3] = 0;
        project_capture(&proj_scratch); ok = proj_pack(&st, &proj_scratch);
        eq_v[1] = 7;
        ok = ok && proj_import(&proj_scratch, &st, sizeof st);
        ok = ok && project_restore_runtime(&proj_scratch) == 0;   /* (V10.58: set when the project is opened, not by the read) */
        ck("   one without it opens flat", ok && !eq_v[0] && !eq_v[1] && !eq_v[2] && !eq_v[3]);
    }
    printf("eq: %s\n", bad ? "FAILED" : "all ok");
    return bad != 0;
}
