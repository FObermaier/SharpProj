#include "pch.h"
#include "CoordinateReferenceSystemList.h"
#include "CoordinateSystem.h"

using namespace SharpProj;
using System::Collections::Generic::List;

int CoordinateReferenceSystemList::AxisCount::get()
{
	int n = __super::AxisCount;
	if (!n)
	{
		GC::KeepAlive(Count); // ensures items

		__super::AxisCount = n = this[0]->AxisCount + this[1]->AxisCount;
	}

	return n;
}

Proj::AxisCollection^ CoordinateReferenceSystemList::Axis::get()
{
	if (!m_axis)
	{
		auto lst = gcnew List<Proj::Axis^>();

		for each (CoordinateReferenceSystem ^ r in this)
		{
			lst->AddRange(r->Axis);
		}

		m_axis = gcnew Proj::AxisCollection(lst);
	}

	return m_axis;
}
