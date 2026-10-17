/* Goxel 3D voxels editor
 *
 * copyright (c) 2026 yberudev <yberudev@gmail.com>
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

#ifndef MCP_BRIDGE_H
#define MCP_BRIDGE_H

/*
 * Local command bridge, used by external tools (for example a Model Context
 * Protocol server) to inspect and edit the scene open in the editor.
 *
 * The bridge is disabled unless a socket path is given, either with the
 * '--mcp-socket' command line option or the GOXEL_MCP_SOCKET environment
 * variable.  See doc/mcp-bridge.md for the protocol.
 */

/*
 * Function: mcp_bridge_init
 * Start listening on a Unix domain socket.
 *
 * Parameters:
 *   path - Socket path.  If NULL, use the GOXEL_MCP_SOCKET environment
 *          variable.  If neither is set, the bridge stays disabled.
 */
void mcp_bridge_init(const char *path);

/*
 * Function: mcp_bridge_iter
 * Serve pending requests.  Must be called once per frame from the main
 * thread.
 */
void mcp_bridge_iter(void);

/*
 * Function: mcp_bridge_release
 * Close the connections and remove the socket file.
 */
void mcp_bridge_release(void);

#endif // MCP_BRIDGE_H
