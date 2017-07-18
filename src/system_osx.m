/* Goxel 3D voxels editor
 *
 * copyright (c) 2017 Guillaume Chereau <guillaume@noctua-software.com>
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
#include <sys/stat.h>
#import <Foundation/Foundation.h>

const char *sys_get_user_dir(void)
{
    static char *ret = NULL;
    NSArray *paths;
    NSString *path;
    if (!ret) {
        paths = NSSearchPathForDirectoriesInDomains(
            NSApplicationSupportDirectory,
            NSUserDomainMask, YES);
        if ([paths count] > 0) {
            path = [paths objectAtIndex: 0];
            asprintf(&ret, "%s/Goxel",
                    [path cStringUsingEncoding:NSASCIIStringEncoding]);
        }
    }
    return ret;
}
