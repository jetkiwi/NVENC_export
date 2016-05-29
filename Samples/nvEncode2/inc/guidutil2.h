#ifndef _GUIDUTIL_H
#define _GUIDUTIL_H

#include<string>
#include<windows.h>
#include<stdint.h>
using namespace std;

namespace guidutil {

	typedef int32_t inttype;
}

struct st_guid_entry
{
		GUID    guid;    // the GUID
		string  s_guid;  // GUID's mnemonic code (in ASCII string format)

		guidutil::inttype value;   // the integer mapped value
		string  s_value; // value's mnemonic code (ASCII)
};

class cls_convert_guid
{
	
public:
	typedef st_guid_entry entry_t;

	cls_convert_guid(
//		const st_guid_entry<inttype> table[],
		const entry_t table[],
		const uint32_t table_size
	);
	~cls_convert_guid();

	// Search by GUID
	bool guid2string(const GUID &guid, string &s) const;
	bool guid2value(const GUID &guid, guidutil::inttype &value) const;
	bool guid2value2string(const GUID &guid, string &s) const;

	// Search by value
	bool value2guid(const guidutil::inttype &value, GUID &guid) const;
	bool value2string(const guidutil::inttype &value, string &s) const;
	bool value2guid2string(const guidutil::inttype &value, string &s) const;

	// Search by index
	bool index2string(const unsigned index, string &s) const;
	bool index2value(const unsigned index, guidutil::inttype &value) const;

	bool compareGUIDs(const GUID &guid1, const GUID &guid2) const;

	const string PrintGUID(const GUID &guid) const;
	
	// utility functions
	uint32_t Size() const { return m_table_size; };
protected:
	const entry_t* get_entry_by_guid(const GUID &guid1) const;
	const entry_t* get_entry_by_value(const guidutil::inttype &value) const;
	const entry_t    *m_table;
	const uint32_t   m_table_size;
}; // class cls_convert_guid



#endif // _GUIDUTIL_H