/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor: 
 *		Jeroen Bakker 
 *		Monique Dewanchand
 */

#ifndef _COM_SocketProxyOperation_h_
#define _COM_SocketProxyOperation_h_

#include "COM_NodeOperation.h"

class SocketProxyOperation : public NodeOperation {
private:
	SocketReader *m_inputOperation;
public:
	SocketProxyOperation(DataType type);
	void executePixel(float output[4], float x, float y, PixelSampler sampler);
	
	void initExecution();
	void deinitExecution();
};

#endif
