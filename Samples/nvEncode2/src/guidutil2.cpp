#include<windows.h>
#include<stdint.h>
#include<string>
#include<cstdio>

#include "guidutil2.h"

using namespace std;

cls_convert_guid::cls_convert_guid(
//	const st_guid_entry<inttype> table[],
	const entry_t table[],
	const uint32_t table_size) :
	m_table(table),
	m_table_size(table_size)
{
};

cls_convert_guid::~cls_convert_guid() {
};

bool cls_convert_guid::guid2string(const GUID &guid, string &s) const {
	const entry_t* table_item = get_entry_by_guid(guid);
	
	if ( table_item ) {
		// found the entry in table
		s = table_item->s_guid;
		return true;
	}

	// not found
	s = PrintGUID(guid); //"unknown guid";
	//s.append( PrintGUID(guid) );

	return false;
};

bool cls_convert_guid::guid2value(const GUID &guid, guidutil::inttype &value) const {
	const entry_t* table_item = get_entry_by_guid(guid);
	
	if ( table_item ) {
		// found the entry in table
		value = table_item->value;
		return true;
	}
	
	// not found
	return false;
};

bool cls_convert_guid::guid2value2string(const GUID &guid, string &s) const {
	guidutil::inttype value;
	bool found = guid2value(guid, value );

	if ( found ) {
		return value2string( value, s);
	}

	// not found
	s = PrintGUID(guid);
	return false;
};

bool cls_convert_guid::value2guid(const guidutil::inttype &value, GUID &guid) const {
	for (uint32_t i = 0; i < m_table_size; ++i ) {
		if ( value == m_table[i].value ) {
			// found a matching entry
			guid = m_table[i].guid;
			return true; 
		}
	}

	// didn't find it
	return false;
};

bool cls_convert_guid::value2string(const guidutil::inttype &value, string &s) const {
	const entry_t* table_item = get_entry_by_value(value);

	if ( table_item ) {
		s = table_item->s_value;
		return true;
	}
	// didn't find it
	s = "unknown value";
	return false;
};

bool cls_convert_guid::value2guid2string(const guidutil::inttype &value, string &s) const {
	GUID guid;
	bool found = value2guid(value, guid );

	if ( found ) {
		return guid2string( guid, s );
	}

	// didn't find it
	s = "unknown value";
	return false;
};

bool cls_convert_guid::index2value(const unsigned index, guidutil::inttype &value) const {
	if ( index < m_table_size ) {
		value = m_table[index].value;
		return true;
	}

	value = 0;
	return false;
}

bool cls_convert_guid::index2string(const unsigned index, string &s) const {
	if ( index < m_table_size ) {
		s = m_table[index].s_value;
		return true;
	}

	s = "illegal index";
	return false;
}

bool cls_convert_guid::compareGUIDs(const GUID &guid1, const GUID &guid2) const
{
    if (guid1.Data1    == guid2.Data1 &&
        guid1.Data2    == guid2.Data2 &&
        guid1.Data3    == guid2.Data3 &&
        guid1.Data4[0] == guid2.Data4[0] &&
        guid1.Data4[1] == guid2.Data4[1] &&
        guid1.Data4[2] == guid2.Data4[2] &&
        guid1.Data4[3] == guid2.Data4[3] &&
        guid1.Data4[4] == guid2.Data4[4] &&
        guid1.Data4[5] == guid2.Data4[5] &&
        guid1.Data4[6] == guid2.Data4[6] &&
        guid1.Data4[7] == guid2.Data4[7]) {
        return true;
    }
    return false;
};


//cls_convert_guid<inttype>::entry_t * 
const st_guid_entry *
	cls_convert_guid::get_entry_by_guid(const GUID &guid) const
{
	for(uint32_t i = 0; i < m_table_size; ++i ) {
		if ( compareGUIDs(guid, m_table[i].guid ) )
			return &m_table[i]; // found the guid, return the entire entry
    }

	// didn't find the desired guid in table
	return NULL; // not found
};

const st_guid_entry*
	cls_convert_guid::get_entry_by_value(const guidutil::inttype &value) const
{
	for(uint32_t i = 0; i < m_table_size; ++i ) {
		if ( value == m_table[i].value )
			return &m_table[i]; // found the guid, return the entire entry
    }

	// didn't find the desired guid in table
	return NULL; // not found
}

const string cls_convert_guid::PrintGUID(const GUID &guid) const
{
	string s;
	char chars[16];
	sprintf(chars, "%08X-", guid.Data1 );
	s.append( chars );

	sprintf(chars, "%04X-", guid.Data2 );
	s.append( chars );

	sprintf(chars, "%04X-", guid.Data3 );
	s.append( chars );

	for(unsigned b = 0; b < 8; ++b ) {
		sprintf(chars, "%02X ", guid.Data4[b] );
		s.append( chars );
	}

	return s;
}
