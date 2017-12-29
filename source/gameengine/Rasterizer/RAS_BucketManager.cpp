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

/** \file gameengine/Rasterizer/RAS_BucketManager.cpp
 *  \ingroup bgerast
 */

#ifdef _MSC_VER
/* don't show these anoying STL warnings */
#  pragma warning (disable:4786)
#endif

#include "RAS_MaterialBucket.h"
#include "RAS_GameObject.h"
#include "RAS_MeshUser.h"
#include "RAS_Polygon.h"
#include "RAS_IPolygonMaterial.h"
#include "RAS_Rasterizer.h"

#include "RAS_BucketManager.h"

#include <algorithm>

RAS_BucketManager::RAS_BucketManager(RAS_IPolyMaterial *textMaterial)
{
	m_text.m_material = textMaterial;
	bool created;
	RAS_MaterialBucket *bucket = FindBucket(m_text.m_material, created);
	m_text.m_arrayBucket = new RAS_DisplayArrayBucket(bucket, nullptr, nullptr, nullptr, nullptr);
}

RAS_BucketManager::~RAS_BucketManager()
{
	delete m_text.m_arrayBucket;
	delete m_text.m_material;

	BucketList& buckets = m_buckets[ALL_BUCKET];
	for (BucketList::iterator it = buckets.begin(), end = buckets.end(); it != end; ++it) {
		delete *it;
	}
	buckets.clear();
}

RAS_MaterialBucket *RAS_BucketManager::FindBucket(RAS_IPolyMaterial *material, bool &bucketCreated)
{
	bucketCreated = false;

	BucketList& buckets = m_buckets[ALL_BUCKET];
	for (BucketList::iterator it = buckets.begin(), end = buckets.end(); it != end; ++it) {
		RAS_MaterialBucket *bucket = *it;
		if (bucket->GetPolyMaterial() == material) {
			return bucket;
		}
	}

	RAS_MaterialBucket *bucket = new RAS_MaterialBucket(material);
	bucketCreated = true;

	const bool useinstancing = false; //material->UseInstancing();
	if (!material->OnlyShadow()) {
		if (material->IsAlpha()) {
			m_buckets[useinstancing ? ALPHA_INSTANCING_BUCKET : ALPHA_BUCKET].push_back(bucket);
			if (material->IsAlphaDepth()) {
				m_buckets[useinstancing ? ALPHA_DEPTH_INSTANCING_BUCKET : ALPHA_DEPTH_BUCKET].push_back(bucket);
			}
		}
		else {
			m_buckets[useinstancing ? SOLID_INSTANCING_BUCKET : SOLID_BUCKET].push_back(bucket);
		}
	}
	if (material->CastsShadows()) {
		if (material->IsAlphaShadow()) {
			m_buckets[useinstancing ? ALPHA_SHADOW_INSTANCING_BUCKET : ALPHA_SHADOW_BUCKET].push_back(bucket);
		}
		else {
			m_buckets[useinstancing ? SOLID_SHADOW_INSTANCING_BUCKET : SOLID_SHADOW_BUCKET].push_back(bucket);
		}
	}

	// Used to free the bucket.
	m_buckets[ALL_BUCKET].push_back(bucket);
	return bucket;
}

RAS_DisplayArrayBucket *RAS_BucketManager::GetTextDisplayArrayBucket() const
{
	return m_text.m_arrayBucket;
}

void RAS_BucketManager::UpdateShaders(RAS_IPolyMaterial *mat)
{
	BucketList& buckets = m_buckets[ALL_BUCKET];
	for (BucketList::iterator it = buckets.begin(), end = buckets.end(); it != end; ++it) {
		RAS_MaterialBucket *bucket = *it;
		if (bucket->GetPolyMaterial() != mat && mat) {
			continue;
		}
		bucket->UpdateShader();
	}
}

void RAS_BucketManager::ReleaseMaterials(RAS_IPolyMaterial *mat)
{
	BucketList& buckets = m_buckets[ALL_BUCKET];
	for (BucketList::iterator it = buckets.begin(), end = buckets.end(); it != end; ++it) {
		RAS_MaterialBucket *bucket = *it;
		if (mat == nullptr || (mat == bucket->GetPolyMaterial())) {
			bucket->GetPolyMaterial()->ReleaseMaterial();
		}
	}
}

/* frees the bucket, only used when freeing scenes */
void RAS_BucketManager::RemoveMaterial(RAS_IPolyMaterial *mat)
{
	for (unsigned short i = 0; i < NUM_BUCKET_TYPE; ++i) {
		BucketList& buckets = m_buckets[i];
		for (BucketList::iterator it = buckets.begin(); it != buckets.end();) {
			RAS_MaterialBucket *bucket = *it;
			if (mat == bucket->GetPolyMaterial()) {
				it = buckets.erase(it);
				if (i == ALL_BUCKET) {
					delete bucket;
				}
			}
			else {
				++it;
			}
		}
	}
}

void RAS_BucketManager::MergeBucketManager(RAS_BucketManager *other)
{
	for (unsigned short i = 0; i < NUM_BUCKET_TYPE; ++i) {
		BucketList& buckets = m_buckets[i];
		BucketList& otherbuckets = other->m_buckets[i];
		buckets.insert(buckets.begin(), otherbuckets.begin(), otherbuckets.end());
		otherbuckets.clear();
	}
}
