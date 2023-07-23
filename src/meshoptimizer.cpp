/* Goxel 3D voxels editor
 *
 * copyright (c) 2023-present Guillaume Chereau <guillaume@noctua-software.com>
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

#include "../ext_src/meshoptimizer/meshoptimizer.h"
#include "../ext_src/meshoptimizer/allocator.cpp"
#include "../ext_src/meshoptimizer/clusterizer.cpp"
#include "../ext_src/meshoptimizer/indexcodec.cpp"
#include "../ext_src/meshoptimizer/indexgenerator.cpp"
#include "../ext_src/meshoptimizer/overdrawanalyzer.cpp"
#include "../ext_src/meshoptimizer/overdrawoptimizer.cpp"
#include "../ext_src/meshoptimizer/simplifier.cpp"
#include "../ext_src/meshoptimizer/spatialorder.cpp"
#include "../ext_src/meshoptimizer/stripifier.cpp"
#include "../ext_src/meshoptimizer/vcacheanalyzer.cpp"
#include "../ext_src/meshoptimizer/vcacheoptimizer.cpp"
#include "../ext_src/meshoptimizer/vertexcodec.cpp"
#include "../ext_src/meshoptimizer/vertexfilter.cpp"
#include "../ext_src/meshoptimizer/vfetchanalyzer.cpp"
#include "../ext_src/meshoptimizer/vfetchoptimizer.cpp"
