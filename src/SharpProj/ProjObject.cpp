#include "pch.h"
#include "ProjObject.h"
#include "ProjException.h"
#include "CoordinateTransform.h"
#include "CoordinateTransformList.h"
#include "CoordinateReferenceSystem.h"
#include "CoordinateReferenceSystemList.h"
#include "CoordinateSystem.h"
#include "Ellipsoid.h"
#include "GeographicCRS.h"
#include "PrimeMeridian.h"
#include "ReferenceFrame.h"
#include "DatumList.h"
#include "Datum.h"

ProjObject^ ProjContext::Create(String^ definition)
{
	if (String::IsNullOrWhiteSpace(definition))
		throw gcnew ArgumentNullException("definition");

	std::string fromStr = utf8_string(definition);
	PJ* pj = proj_create(this, fromStr.c_str());

	if (!pj)
		throw ConstructException();

	return Create(pj);
}

ProjObject^ ProjContext::Create(...array<String^>^ from)
{
	char** lst = new char* [from->Length + 1];
	for (int i = 0; i < from->Length; i++)
	{
		std::string fromStr = utf8_string(from[i]);
		lst[i] = _strdup(fromStr.c_str());
	}
	lst[from->Length] = 0;

	try
	{
		PJ* pj = proj_create_argv(this, from->Length, lst);

		if (!pj)
			throw ConstructException();

		return Create(pj);
	}
	finally
	{
		for (int i = 0; i < from->Length; i++)
		{
			free(lst[i]);
		}
		delete[] lst;
	}
}

ProjObject^ ProjContext::Create(PJ* pj)
{
	if (!pj)
		throw gcnew ArgumentNullException("pj");

	switch ((ProjType)proj_get_type(pj))
	{
	case ProjType::Ellipsoid:
		return gcnew Ellipsoid(this, pj);

	case ProjType::PrimeMeridian:
		return gcnew Proj::PrimeMeridian(this, pj);

	case ProjType::GeodeticReferenceFrame:
	case ProjType::DynamicGeodeticReferenceFrame:
	case ProjType::VerticalReferenceFrame:
	case ProjType::DynamicVerticalReferenceFrame:
		return gcnew Proj::ReferenceFrame(this, pj);

	case ProjType::DatumEnsamble:
		return gcnew Proj::DatumList(this, pj);

	case ProjType::GeographicCrs: // Never used. Only inherited
	case ProjType::Geographic2DCrs:
	case ProjType::Geographic3DCrs:
		return gcnew GeographicCRS(this, pj);

	case ProjType::GeodeticCrs:
	case ProjType::GeocentricCrs:
		return gcnew GeodeticCRS(this, pj);
	
	case ProjType::CompoundCrs:
		return gcnew CoordinateReferenceSystemList(this, pj);

	case ProjType::CRS: // abstract
	case ProjType::VerticalCrs:
	case ProjType::ProjectedCrs:
	case ProjType::TemporalCrs:
	case ProjType::EngineeringCrs:	
	case ProjType::OtherCrs:
		return gcnew CoordinateReferenceSystem(this, pj);

	case ProjType::BoundCrs:
		return gcnew BoundCRS(this, pj);

	case ProjType::Conversion:
	case ProjType::Transformation:
	case ProjType::OtherCoordinateTransform:
		return gcnew CoordinateTransform(this, pj);

	case ProjType::ConcatenatedOperation:
		return gcnew CoordinateTransformList(this, pj);

	case ProjType::TemporalDatum:
	case ProjType::EngineeringDatum:
	case ProjType::ParametricDatum:
		return gcnew Proj::Datum(this, pj);

	case ProjType::ChooseTransform:
	case ProjType::CoordinateSystem:
		throw gcnew InvalidOperationException(); // Never returned by proj, but needed for sensible API

	case ProjType::Unknown:		
	default:
		CoordinateSystemType cst = (CoordinateSystemType)proj_cs_get_type(this, pj);

		if (cst != CoordinateSystemType::Unknown)
			return gcnew CoordinateSystem(this, pj);

		ClearError(pj);


		if (!strcmp(proj_get_name(pj), "Transformation pipeline manager"))
		{
			return gcnew CoordinateTransform(this, pj);
		}

		return gcnew ProjObject(this, pj);
	}
}

generic<typename T> where T : ProjObject
T ProjContext::Create(PJ* pj)
{
	ProjObject^ o = Create(pj);

	return safe_cast<T>(o);
}

ProjObject^ ProjObject::Create(String^ definition, [Optional]ProjContext^ ctx)
{
	if (!ctx)
		ctx = gcnew ProjContext();

	return ctx->Create(definition);
}

ProjObject^ ProjObject::Create(array<String^>^ from, [Optional]ProjContext^ ctx)
{
	if (!ctx)
		ctx = gcnew ProjContext();

	return ctx->Create(from);
}

IdentifierList^ ProjObject::Identifiers::get()
{
	if (!m_idList && proj_get_id_auth_name(this, 0))
		m_idList = gcnew IdentifierList(this);

	return m_idList;
}

Identifier^ IdentifierList::default::get(int index)
{
	if (index < 0 || m_Items ? (index >= m_Items->Length) : !proj_get_id_auth_name(m_object, index))
		throw gcnew IndexOutOfRangeException();

	if (m_Items)
	{
		if (!m_Items[index])
			m_Items[index] = gcnew Identifier(m_object, index);

		return m_Items[index];
	}
	else
	{
		// Count still unknown
		return gcnew Identifier(m_object, index);
	}
}

int IdentifierList::Count::get()
{
	if (m_Items)
		return m_Items->Length;

	for (int i = 0; i < 256 /* some MAX */; i++)
	{
		if (!proj_get_id_auth_name(m_object, i))
		{
			m_Items = gcnew array<Identifier^>(i);
			return i;
		}
	}
	m_Items = Array::Empty<Identifier^>();
	return 0;
}

System::Collections::Generic::IEnumerator<Identifier^>^ IdentifierList::GetEnumerator()
{
	for(int i = 0; i < Count /* initializes m_Items if necessary */; i++)
	{
		if (!m_Items[i])
			m_Items[i] = gcnew Identifier(m_object, i);
	}

	return static_cast<System::Collections::Generic::IEnumerable<Identifier^>^>(m_Items)->GetEnumerator();
}

String^ Identifier::Authority::get()
{
	if (!m_authority)
	{
		const char* auth = proj_get_id_auth_name(m_object, m_index);

		m_authority = auth ? gcnew String(auth) : nullptr;
	}
	return m_authority;
}

String^ Identifier::Name::get()
{
	if (!m_code)
	{
		const char* code = proj_get_id_code(m_object, m_index);

		m_code = code ? gcnew String(code) : nullptr;
	}
	return m_code;
}
