//------------------------------------------------------------------------------
/*
 This file is part of chainsqld: https://github.com/chainsql/chainsqld
 Copyright (c) 2016-2018 Peersafe Technology Co., Ltd.
 
	chainsqld is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.
 
	chainsqld is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
 */
//==============================================================================

#ifndef RIPPLE_APP_MISC_SQLDATATYPE_H_INCLUDED
#define RIPPLE_APP_MISC_SQLDATATYPE_H_INCLUDED

#include <string>
#include <cstring> // std::strcmp
#include <cassert> // assert

namespace ripple {

// only hole-type, no sense
class InnerDateTime
{
public:
	InnerDateTime() {}
	~InnerDateTime() {}
};

class InnerDate {
public:
	InnerDate() {}
	~InnerDate() {}
};

class InnerDecimal
{
public:
	InnerDecimal(int length, int accuracy)
		: length_(length)
		, accuracy_(accuracy) {
	}

	~InnerDecimal() {}
	const int length() {
		return length_;
	}

	const int length() const {
		return length_;
	}

	const int accuracy() {
		return accuracy_;
	}

	const int accuracy() const {
		return accuracy_;
	}

	void update(const InnerDecimal& v) {
		length_ = v.length_;
		accuracy_ = v.accuracy_;
	}

	InnerDecimal& operator =(const InnerDecimal& value) {
		length_ = value.length_;
		accuracy_ = value.accuracy_;
		return *this;
	}

	bool operator ==(const InnerDecimal& value) {
		return length_ == value.length_ && accuracy_ == value.accuracy_;
	}

private:
	InnerDecimal() {}
	int length_;	// length of decimal
	int accuracy_;	// accuracy of decimal
};

class FieldValue {
public:
	explicit FieldValue()
		: value_type_(INNER_UNKOWN) {};

	explicit FieldValue(const std::string& value)
		: value_type_(STRING) {
		value_.str = new std::string;
		if (value_.str) {
			value_.str->assign(value);
		}
	}

	enum { fVARCHAR, fCHAR, fTEXT, fBLOB };
	explicit FieldValue(const std::string& value, int flag)
		: value_type_(STRING) {

		if (flag == fVARCHAR)
			value_type_ = VARCHAR;
		else if (flag == fCHAR)
			value_type_ = CHAR;
		else if (flag == fTEXT)
			value_type_ = TEXT;
		else if (flag == fBLOB)
			value_type_ = BLOB;

		value_.str = new std::string;
		if (value_.str) {
			value_.str->assign(value);
		}
	}

	explicit FieldValue(const int value)
		: value_type_(INT) {
		value_.i = value;
	}

	explicit FieldValue(const unsigned int value)
		: value_type_(UINT) {
		value_.ui = value;
	}

	explicit FieldValue(const float f)
		: value_type_(FLOAT) {
		value_.f = f;
	}

	explicit FieldValue(const double d)
		: value_type_(DOUBLE) {
		value_.d = d;
	}

	explicit FieldValue(const int64_t value)
		: value_type_(LONG64) {
		value_.i64 = value;
	}

	explicit FieldValue(const InnerDateTime& datetime)
		: value_type_(DATETIME) {
		value_.datetime = nullptr;
	}

	explicit FieldValue(const InnerDate& date)
		: value_type_(DATE) {
		value_.date = nullptr;
	}

	explicit FieldValue(const InnerDecimal& d)
		: value_type_(DECIMAL) {
		value_.decimal = new InnerDecimal(d.length(), d.accuracy());
	}

	explicit FieldValue(const FieldValue& value)
		: value_type_(value.value_type_) {
		assign(value);
	}

	FieldValue& operator =(const FieldValue& value) {
		value_type_ = value.value_type_;
		assign(value);
		return *this;
	}

	void assign(const FieldValue& value) {
		if (value_type_ == INT) {
			value_.i = value.value_.i;
		}
		else if (value_type_ == UINT) {
			value_.ui = value.value_.ui;
		}
		else if (value_type_ == STRING || value_type_ == VARCHAR
			|| value_type_ == TEXT || value_type_ == BLOB
			|| value_type_ == CHAR) {

			value_.str = new std::string;
			if (value_.str) {
				value_.str->assign(value.value_.str->c_str());
			}
		}
		else if (value_type_ == DATETIME) {
			value_.datetime = value.value_.datetime;
		}
		else if (value_type_ == DATE) {
			value_.date = value.value_.date;
		} else if (value_type_ == LONG64) {
			value_.i64 = value.value_.i64;
		}
		else if (value_type_ == FLOAT) {
			value_.f = value.value_.f;
		}
		else if (value_type_ == DOUBLE) {
			value_.d = value.value_.d;
		}
		else if (value_type_ == DECIMAL) {
			value_.decimal = new InnerDecimal(value.value_.decimal->length(),
				value.value_.decimal->accuracy());
		}
	}

	FieldValue& operator =(const std::string& value) {
		value_type_ = STRING;
		value_.str = new std::string;
		if (value_.str) {
			value_.str->assign(value);
		}
		return *this;
	}

	FieldValue& operator =(const int value) {
		value_type_ = INT;
		value_.i = value;
		return *this;
	}

	FieldValue& operator =(const unsigned int value) {
		value_type_ = UINT;
		value_.ui = value;
		return *this;
	}

	FieldValue& operator =(const float value) {
		value_type_ = FLOAT;
		value_.f = value;
		return *this;
	}

	FieldValue& operator =(const double value) {
		value_type_ = DOUBLE;
		value_.d = value;
		return *this;
	}

	FieldValue& operator =(const InnerDateTime& value) {
		value_type_ = DATETIME;
		value_.datetime = nullptr;
		return *this;
	}

	FieldValue& operator =(const InnerDate& value) {
		value_type_ = DATE;
		value_.date = nullptr;
		return *this;
	}

	FieldValue& operator =(const InnerDecimal& value) {
		value_type_ = DECIMAL;
		value_.decimal = new InnerDecimal(value.length(), value.accuracy());
		return *this;
	}

	FieldValue& operator =(const int64_t value) {
		value_type_ = LONG64;
		value_.i64 = value;
		return *this;
	}

	bool operator ==(const FieldValue& r) const {
		bool eq = false;
		assert(value_type_ == r.value_type_);
		if (value_type_ != r.value_type_)
			return eq;

		if (value_type_ == INT)
			eq = (value_.i == r.value_.i);
		else if (value_type_ == UINT)
			eq = (value_.ui == r.value_.ui);
		else if (value_type_ == FLOAT)
			eq = (value_.f == r.value_.f);
		else if (value_type_ == DOUBLE)
			eq = (value_.d == r.value_.d);
		else if (value_type_ == DECIMAL)
			eq = (*value_.decimal) == (*r.value_.decimal);
		else if (value_type_ == LONG64)
			eq = (value_.i64 == r.value_.i64);
		else if (value_type_ == STRING || value_type_ == VARCHAR
			|| value_type_ == TEXT || value_type_ == BLOB
			|| value_type_ == CHAR) {
			if (std::strcmp(value_.str->c_str(), r.value_.str->c_str()) == 0)
				eq = true;
		}
		else if (value_type_ == DATETIME || value_type_ == DATE) // TODO ??
			eq = false;

		return eq;
	}

	bool operator <(const FieldValue& r) const {
		bool eq = false;
		assert(value_type_ == r.value_type_);
		if (value_type_ != r.value_type_)
			return eq;

		if (value_type_ == INT)
			eq = (value_.i < r.value_.i);
		else if (value_type_ == UINT)
			eq = (value_.ui < r.value_.ui);
		else if (value_type_ == FLOAT)
			eq = (value_.f < r.value_.f);
		else if (value_type_ == DOUBLE)
			eq = (value_.d < r.value_.d);
		else if (value_type_ == LONG64)
			eq = (value_.i64 < r.value_.i64);
		else if (value_type_ == STRING || value_type_ == VARCHAR
			|| value_type_ == TEXT || value_type_ == BLOB
			|| value_type_ == CHAR) {
			if (std::strcmp(value_.str->c_str(), r.value_.str->c_str()) < 0)
				eq = true;
		}
		else if (value_type_ == DECIMAL) {
			assert(0);
		}
		else if (value_type_ == DATETIME || value_type_ == DATE) // TODO ??
			eq = false;

		return eq;
	}

	bool operator <=(const FieldValue& r) const {
		bool eq = false;
		assert(value_type_ == r.value_type_);
		if (value_type_ != r.value_type_)
			return eq;

		if (value_type_ == INT)
			eq = (value_.i <= r.value_.i);
		else if (value_type_ == UINT)
			eq = (value_.ui <= r.value_.ui);
		else if (value_type_ == FLOAT)
			eq = (value_.f <= r.value_.f);
		else if (value_type_ == DOUBLE)
			eq = (value_.d <= r.value_.d);
		else if (value_type_ == DECIMAL)
			assert(0);
		else if (value_type_ == LONG64)
			eq = (value_.i64 <= r.value_.i64);
		else if (value_type_ == STRING || value_type_ == VARCHAR
			|| value_type_ == TEXT || value_type_ == BLOB
			|| value_type_ == CHAR) {
			if (std::strcmp(value_.str->c_str(), r.value_.str->c_str()) < 0
				||std::strcmp(value_.str->c_str(), r.value_.str->c_str()) == 0)
				eq = true;
		}
		else if (value_type_ == DATETIME || value_type_ == DATE) // TODO ??
			eq = false;

		return eq;
	}

	bool operator >(const FieldValue& r) const {
		bool eq = false;
		assert(value_type_ == r.value_type_);
		if (value_type_ != r.value_type_)
			return eq;

		if (value_type_ == INT)
			eq = (value_.i > r.value_.i);
		else if (value_type_ == UINT)
			eq = (value_.ui > r.value_.ui);
		else if (value_type_ == FLOAT)
			eq = (value_.f > r.value_.f);
		else if (value_type_ == DOUBLE)
			eq = (value_.d > r.value_.d);
		else if (value_type_ == DECIMAL)
			assert(0);
		else if (value_type_ == LONG64)
			eq = (value_.i64 > r.value_.i64);
		else if (value_type_ == STRING || value_type_ == VARCHAR
			|| value_type_ == TEXT || value_type_ == BLOB
			|| value_type_ == CHAR) {
			if (std::strcmp(value_.str->c_str(), r.value_.str->c_str()) > 0)
				eq = true;
		}
		else if (value_type_ == DATETIME || value_type_ == DATE) // TODO ??
			eq = false;

		return eq;
	}

	bool operator >=(const FieldValue& r) const {
		bool eq = false;
		assert(value_type_ == r.value_type_);
		if (value_type_ != r.value_type_)
			return eq;

		if (value_type_ == INT)
			eq = (value_.i >= r.value_.i);
		else if (value_type_ == UINT)
			eq = (value_.ui >= r.value_.ui);
		else if (value_type_ == FLOAT)
			eq = (value_.f >= r.value_.f);
		else if (value_type_ == DOUBLE)
			eq = (value_.d >= r.value_.d);
		else if (value_type_ == DECIMAL)
			assert(0);
		else if (value_type_ == LONG64)
			eq = (value_.i64 >= r.value_.i64);
		else if (value_type_ == STRING || value_type_ == VARCHAR
			|| value_type_ == TEXT || value_type_ == BLOB
			|| value_type_ == CHAR) {
			if (std::strcmp(value_.str->c_str(), r.value_.str->c_str()) > 0
				|| std::strcmp(value_.str->c_str(), r.value_.str->c_str()) == 0)
				eq = true;
		}
		else if (value_type_ == DATETIME || value_type_ == DATE) // TODO ??
			eq = false;

		return eq;
	}

	~FieldValue() {
		if ((value_type_ == STRING || value_type_ == VARCHAR
			|| value_type_ == TEXT || value_type_ == BLOB
			|| value_type_ == CHAR)
			&& value_.str) {
			delete value_.str;
			value_.str = nullptr;
		}
		else if (value_type_ == DECIMAL) {
			delete value_.decimal;
			value_.decimal = nullptr;
		}
	}

	bool isNumeric() {
		return (value_type_ == INT || value_type_ == UINT || value_type_ == LONG64
			|| value_type_ == FLOAT || value_type_ == DOUBLE || value_type_ == DECIMAL);
	}

	bool isNumeric() const {
		return (value_type_ == INT || value_type_ == UINT || value_type_ == LONG64
			|| value_type_ == FLOAT || value_type_ == DOUBLE || value_type_ == DECIMAL);
	}

	bool isInt() {
		return value_type_ == INT;
	}

	bool isInt() const {
		return value_type_ == INT;
	}

	bool isUint() {
		return value_type_ == UINT;
	}

	bool isUint() const {
		return value_type_ == UINT;
	}

	bool isFloat() {
		return value_type_ == FLOAT;
	}

	bool isFloat() const {
		return value_type_ == FLOAT;
	}

	bool isDouble() {
		return value_type_ == DOUBLE;
	}

	bool isDouble() const {
		return value_type_ == DOUBLE;
	}

	bool isDecimal() {
		return value_type_ == DECIMAL;
	}

	bool isDecimal() const {
		return value_type_ == DECIMAL;
	}

	bool isInt64() {
		return value_type_ == LONG64;
	}

	bool isInt64() const {
		return value_type_ == LONG64;
	}

	bool isString() {
		return value_type_ == STRING;
	}

	bool isString() const {
		return value_type_ == STRING;
	}

	bool isVarchar() {
		return value_type_ == VARCHAR;
	}

	bool isVarchar() const {
		return value_type_ == VARCHAR;
	}

	bool isChar() {
		return value_type_ == CHAR;
	}

	bool isChar() const {
		return value_type_ == CHAR;
	}

	bool isText() {
		return value_type_ == TEXT;
	}

	bool isText() const {
		return value_type_ == TEXT;
	}

	bool isBlob() {
		return value_type_ == BLOB;
	}

	bool isBlob() const {
		return value_type_ == BLOB;
	}

	bool isDateTime() {
		return value_type_ == DATETIME;
	}

	bool isDateTime() const {
		return value_type_ == DATETIME;
	}

	bool isDate() {
		return value_type_ == DATE;
	}

	bool isDate() const {
		return value_type_ == DATE;
	}

	const int& asInt() {
		return value_.i;
	}

	const int& asInt() const {
		return value_.i;
	}

	const unsigned int& asUint() {
		return value_.ui;
	}

	const unsigned int& asUint() const {
		return value_.ui;
	}

	const int64_t& asInt64() {
		return value_.i64;
	}

	const int64_t& asInt64() const {
		return value_.i64;
	}

	const float& asFloat() {
		return value_.f;
	}

	const float& asFloat() const {
		return value_.f;
	}

	const double& asDouble() {
		return value_.d;
	}

	const double& asDouble() const {
		return value_.d;
	}

	InnerDecimal& asDecimal() {
		return *value_.decimal;
	}

	const InnerDecimal& asDecimal() const {
		return *value_.decimal;
	}

	const std::string& asString() {
		return *value_.str;
	}

	const std::string& asString() const {
		return *value_.str;
	}


	void update(const InnerDecimal& d) {
		assert(value_.decimal);
		value_.decimal->update(d);
	}

private:

	enum inner_type {
		INNER_UNKOWN,
		INT,
		UINT,
		FLOAT,
		DOUBLE,
		LONG64,
		DECIMAL,
		DATETIME,
		DATE,
		TEXT,
		VARCHAR,
		CHAR,
		BLOB,
		STRING
	};

	int value_type_;
	union inner_value {
		int i;
		unsigned int ui;
		int64_t i64;
		float f;
		double d;
		InnerDateTime *datetime;
		InnerDate *date;
		InnerDecimal *decimal;
		std::string *str; // varchar/text/blob/decimal
	} value_;
};

} // namespace ripple

#endif // RIPPLE_APP_MISC_SQLDATATYPE_H_INCLUDED
