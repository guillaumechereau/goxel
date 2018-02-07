#include "vec.h"
#include <float.h>

static const int EUL_ORDERS[][4] = {
    // i, j, k, n
    {0, 1, 2, 0}, // XYZ
    {0, 2, 1, 1}, // XZY
    {1, 0, 2, 1}, // YXZ
    {1, 2, 0, 0}, // YZX
    {2, 0, 1, 0}, // ZXY
    {2, 1, 0, 1}  // ZYX
};

void mat3_normalize_(mat3_t *m)
{
    int i;
    for (i = 0; i < 3; i++)
        vec3_normalize(m->vecs[i].v, m->vecs[i].v);
}

static void mat3_to_eul2_(const mat3_t *m, int order,
                          float e1[3], float e2[3])
{

    const int *r = EUL_ORDERS[order];
    int i = r[0], j = r[1], k = r[2];
    int parity = r[3];
    real_t cy = hypot(m->v2[i][i], m->v2[i][j]);
    if (cy > 16.0f * FLT_EPSILON) {
        e1[i] = atan2(m->v2[j][k], m->v2[k][k]);
        e1[j] = atan2(-m->v2[i][k], cy);
        e1[k] = atan2(m->v2[i][j], m->v2[i][i]);
        e2[i] = atan2(-m->v2[j][k], -m->v2[k][k]);
        e2[j] = atan2(-m->v2[i][k], -cy);
        e2[k] = atan2(-m->v2[i][j], -m->v2[i][i]);
    } else {
        e1[i] = atan2(-m->v2[k][j], m->v2[j][j]);
        e1[j] = atan2(-m->v2[i][k], cy);
        e1[k] = 0.0;
        *e2 = *e1;
    }
    if (parity) {
        vec3_imul(e1, -1);
        vec3_imul(e2, -1);
    }
}

void mat3_to_eul(mat3_t m, int order, float e[3])
{
    float e1[3], e2[3];
    mat3_t n = m;
    mat3_normalize_(&n);

    mat3_to_eul2_(&n, order, e1, e2);

    // Pick best.
    if (    fabs(e1[0]) + fabs(e1[1]) + fabs(e1[2]) >
            fabs(e2[0]) + fabs(e2[1]) + fabs(e2[2])) {
        vec3_copy(e1, e);
    } else {
        vec3_copy(e2, e);
    }
}

void quat_to_mat3_(const quat_t *q, mat3_t *m)
{
    real_t q0, q1, q2, q3, qda, qdb, qdc, qaa, qab, qac, qbb, qbc, qcc;

    q0 = M_SQRT2 * q->w;
    q1 = M_SQRT2 * q->x;
    q2 = M_SQRT2 * q->y;
    q3 = M_SQRT2 * q->z;

    qda = q0 * q1;
    qdb = q0 * q2;
    qdc = q0 * q3;
    qaa = q1 * q1;
    qab = q1 * q2;
    qac = q1 * q3;
    qbb = q2 * q2;
    qbc = q2 * q3;
    qcc = q3 * q3;

    m->v2[0][0] = (1.0 - qbb - qcc);
    m->v2[0][1] = (qdc + qab);
    m->v2[0][2] = (-qdb + qac);

    m->v2[1][0] = (-qdc + qab);
    m->v2[1][1] = (1.0 - qaa - qcc);
    m->v2[1][2] = (qda + qbc);

    m->v2[2][0] = (qdb + qac);
    m->v2[2][1] = (-qda + qbc);
    m->v2[2][2] = (1.0 - qaa - qbb);
}

void eul_to_quat_(const float e[3], int order, quat_t *q)
{
    const int *r = EUL_ORDERS[order];
    int i = r[0], j = r[1], k = r[2];
    int parity = r[3];
    double a[3];
    double ti, tj, th, ci, cj, ch, si, sj, sh, cc, cs, sc, ss;

    ti = e[i] * 0.5f;
    tj = e[j] * (parity ? -0.5f : 0.5f);
    th = e[k] * 0.5f;
    ci = cos(ti);
    cj = cos(tj);
    ch = cos(th);
    si = sin(ti);
    sj = sin(tj);
    sh = sin(th);
    cc = ci * ch;
    cs = ci * sh;
    sc = si * ch;
    ss = si * sh;

    a[i] = cj * sc - sj * cs;
    a[j] = cj * ss + sj * cc;
    a[k] = cj * cs - sj * sc;

    q->w = cj * cc + sj * ss;
    q->x = a[0];
    q->y = a[1];
    q->z = a[2];

    if (parity) q->v[j + 1] = -q->v[j + 1];
}
