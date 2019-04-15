/* Copyright (c) 2019 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

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

void mat3_normalize_(const float m[3][3], float out[3][3])
{
    int i;
    for (i = 0; i < 3; i++)
        vec3_normalize(m[i], out[i]);
}

void mat3_to_eul2(const float m[3][3], int order, float e1[3], float e2[3])
{

    const int *r = EUL_ORDERS[order];
    int i = r[0], j = r[1], k = r[2];
    int parity = r[3];
    float cy = hypot(m[i][i], m[i][j]);
    float n[3][3];

    mat3_normalize_(m, n);
    if (cy > 16.0f * FLT_EPSILON) {
        e1[i] = atan2(m[j][k], m[k][k]);
        e1[j] = atan2(-m[i][k], cy);
        e1[k] = atan2(m[i][j], m[i][i]);
        e2[i] = atan2(-m[j][k], -m[k][k]);
        e2[j] = atan2(-m[i][k], -cy);
        e2[k] = atan2(-m[i][j], -m[i][i]);
    } else {
        e1[i] = atan2(-m[k][j], m[j][j]);
        e1[j] = atan2(-m[i][k], cy);
        e1[k] = 0.0;
        vec3_copy(e1, e2);
    }
    if (parity) {
        vec3_imul(e1, -1);
        vec3_imul(e2, -1);
    }
}

void mat3_to_eul(const float m[3][3], int order, float e[3])
{
    float e1[3], e2[3];
    mat3_to_eul2(m, order, e1, e2);

    // Pick best.
    if (    fabs(e1[0]) + fabs(e1[1]) + fabs(e1[2]) >
            fabs(e2[0]) + fabs(e2[1]) + fabs(e2[2])) {
        vec3_copy(e2, e);
    } else {
        vec3_copy(e1, e);
    }
}

void quat_to_mat3(const float q[4], float m[3][3])
{
    float q0, q1, q2, q3, qda, qdb, qdc, qaa, qab, qac, qbb, qbc, qcc;

    q0 = M_SQRT2 * q[0];
    q1 = M_SQRT2 * q[1];
    q2 = M_SQRT2 * q[2];
    q3 = M_SQRT2 * q[3];

    qda = q0 * q1;
    qdb = q0 * q2;
    qdc = q0 * q3;
    qaa = q1 * q1;
    qab = q1 * q2;
    qac = q1 * q3;
    qbb = q2 * q2;
    qbc = q2 * q3;
    qcc = q3 * q3;

    m[0][0] = (1.0 - qbb - qcc);
    m[0][1] = (qdc + qab);
    m[0][2] = (-qdb + qac);

    m[1][0] = (-qdc + qab);
    m[1][1] = (1.0 - qaa - qcc);
    m[1][2] = (qda + qbc);

    m[2][0] = (qdb + qac);
    m[2][1] = (-qda + qbc);
    m[2][2] = (1.0 - qaa - qbb);
}

void eul_to_quat(const float e[3], int order, float q[4])
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

    q[0] = cj * cc + sj * ss;
    q[1] = a[0];
    q[2] = a[1];
    q[3] = a[2];

    if (parity) q[j + 1] = -q[j + 1];
}
