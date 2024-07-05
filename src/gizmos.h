/* Goxel 3D voxels editor
 *
 * copyright (c) 2024-present Guillaume Chereau <guillaume@noctua-software.com>
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

#ifndef GIZMOS_H
#define GIZMOS_H

/*
 * box_edit
 * Render a box that can be edited with the mouse.
 *
 * This is used for the move and selection tools.
 * Still a bit experimental.
 *
 * Parameters:
 *   box    - The box we want to edit.
 *   mode   - 0: move, 1: resize.
 *   transf - Receive the output transformation.
 *   first  - Set to true if the edit is the first one.
 *
 * Return on of:
 *   0 (no action)
 *   GESTURE3D_BEGIN
 *   GESTURE3D_UPDATE
 *   GESTURE3D_END
 */
int box_edit(const float box[4][4], int mode, float transf[4][4]);

bool box_edit_is_active(void);


#endif // GIZMOS_H
