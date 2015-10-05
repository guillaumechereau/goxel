/* Goxel 3D voxels editor
 *
 * copyright (c) 2015 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Goxel is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.

 * Goxel is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.

 * You should have received a copy of the GNU General Public License along with
 * goxel.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "goxel.h"
#include <time.h>

static bool g_running = false;
static profiler_block_t *g_blocks = NULL;
static profiler_block_t *g_current_block = NULL;
static profiler_block_t g_root_block = {"root"};

#ifndef __MACH__
static int64_t get_clock(void)
{
    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);
    return (int64_t)tp.tv_sec * 1000 * 1000 * 1000
         + (int64_t)tp.tv_nsec;
}
#else

// Apparently clock_gettime does not exists on OSX.
#include <sys/time.h>
static int64_t get_clock(void)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return (int64_t)now.tv_sec * 1000 * 1000 * 1000 +
           (int64_t)now.tv_usec * 1000;
}
#endif

void profiler_start(void)
{
    profiler_block_t *b, *tmp;
    LL_FOREACH_SAFE(g_blocks, b, tmp) {
        b->count = b->tot_time = b->self_time = b->depth = 0;
        LL_DELETE(g_blocks, b);
    }
    g_running = true;
}

void profiler_stop(void)
{
    g_running = false;;
}

void profiler_tick(void)
{
    profiler_block_t *block = &g_root_block;
    if (!g_running) profiler_start();
    assert(block->depth <= 1);
    if (!block->depth)
        profiler_enter_(block);
    else
        profiler_exit_(block);
}

static int sort_cmp(const profiler_block_t *a, const profiler_block_t *b)
{
    if (a == &g_root_block) return -1;
    if (b == &g_root_block) return +1;
    return a->self_time < b->self_time ? +1 :
           a->self_time < b->self_time ? -1 :
           0;
}

void profiler_report()
{
    profiler_block_t *b;
    float time;
    int percent;
    LL_SORT(g_blocks, sort_cmp);
    LL_FOREACH(g_blocks, b) {
        time = b->tot_time / (1000.0 * 1000.0);
        percent = b->tot_time * 100 / g_blocks->tot_time;
        LOG_I("%s count:%d time:%.2fms (%d%%)",
                b->name, b->count, time, percent);
    }
}

profiler_block_t *profiler_get_blocks()
{
    LL_SORT(g_blocks, sort_cmp);
    if (!g_blocks->count) return NULL;  // No significant data.
    return g_blocks;
}

void profiler_enter_(profiler_block_t *block)
{
    if (!g_running) return;
    if (!block->depth && !block->count) {
        LL_APPEND(g_blocks, block);
    }
    if (block->depth++) return;
    block->enter_time = get_clock();
    block->parent = g_current_block;
    g_current_block = block;
}

void profiler_exit_(profiler_block_t *block)
{
    if (!g_running) return;
    int64_t time;
    block->depth--;
    if (block->depth) return;
    time = get_clock() - block->enter_time;
    block->tot_time += time;
    block->self_time += time;
    block->count++;
    if (block->parent)
        block->parent->self_time -= time;
    g_current_block = block->parent;
}
