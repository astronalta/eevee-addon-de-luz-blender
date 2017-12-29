/*
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/Rasterizer/RAS_DisplayArrayBucket.cpp
 *  \ingroup bgerast
 */

#include "RAS_DisplayArrayBucket.h"
#include "RAS_MaterialBucket.h"
#include "RAS_MaterialShader.h"
#include "RAS_GameObject.h"
#include "RAS_Deformer.h"
#include "RAS_Rasterizer.h"
#include "RAS_BucketManager.h"

#include <algorithm>

#ifdef _MSC_VER
#  pragma warning (disable:4786)
#endif

#ifdef WIN32
#  include <windows.h>
#endif // WIN32

RAS_DisplayArrayBucket::RAS_DisplayArrayBucket(RAS_MaterialBucket *bucket, RAS_IDisplayArray *array,
											   RAS_GameObject *mesh, RAS_MeshMaterial *meshmat, RAS_Deformer *deformer)
	:m_bucket(bucket),
	m_displayArray(array),
	m_mesh(mesh),
	m_meshMaterial(meshmat),
	m_deformer(deformer)
{
	//m_bucket->AddDisplayArrayBucket(this);
}

RAS_DisplayArrayBucket::~RAS_DisplayArrayBucket()
{
	//m_bucket->RemoveDisplayArrayBucket(this);
}

RAS_MaterialBucket *RAS_DisplayArrayBucket::GetBucket() const
{
	return m_bucket;
}

RAS_IDisplayArray *RAS_DisplayArrayBucket::GetDisplayArray() const
{
	return m_displayArray;
}

RAS_GameObject *RAS_DisplayArrayBucket::GetMesh() const
{
	return m_mesh;
}

RAS_MeshMaterial *RAS_DisplayArrayBucket::GetMeshMaterial() const
{
	return m_meshMaterial;
}

bool RAS_DisplayArrayBucket::UseBatching() const
{
	return (m_displayArray && m_displayArray->GetType() == RAS_IDisplayArray::BATCHING);
}

void RAS_DisplayArrayBucket::ChangeMaterialBucket(RAS_MaterialBucket *bucket)
{
	m_bucket = bucket;
}
