// Copyright (C) 2005 Dave Griffiths
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

#ifndef N_LINEPRIM
#define N_LINEPRIM

#include "PolyPrimitive.h"

namespace Fluxus
{

//////////////////////////////////////////////////////
/// The RibbonPrimitive is constructed from line vertices
/// and widths, and generates geometry which is constantly
/// camera facing. 
class RibbonPrimitive : public Primitive
{
public:
	RibbonPrimitive();
	RibbonPrimitive(const RibbonPrimitive &other);
	virtual ~RibbonPrimitive();
	
	///////////////////////////////////////////////////
	///@name Primitive Interface
	///@{
	virtual RibbonPrimitive* Clone() const;
	virtual void Render();
	virtual dBoundingBox GetBoundingBox(const dMatrix &space);
	virtual void ApplyTransform(bool ScaleRotOnly=false);
	virtual string GetTypeName() { return "RibbonPrimitive"; }
	virtual Evaluator *MakeEvaluator() { return NULL; }
	///@}

    void SetInverseNormals(bool s) { m_InverseNormals=s; }

protected:

	virtual void PDataDirty();

private:
	void Realloc();

	vector<dVector> *m_VertData;
	vector<dColour> *m_ColData;
	vector<float> *m_WidthData;

	vector<dVector> *m_Vertices;
	vector<dVector> *m_Normals;
	vector<dColour> *m_VertCols;
	vector<dVector> *m_TexCoords;

    bool m_InverseNormals;
};

}

#endif
