/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef _ENV_H
#define _ENV_H

/**********************************************************************

The Env object maintains a collection of environment settings, e.g. for
a process that we are about to exec.  Environment values may be fed
into the Env object in several formats.

Example:

Env envobj;
// Add env settings from job ClassAd:
envobj.MergeFrom(ad)
// Add env settings in raw V2 syntax.
envobj.MergeFromV2Raw("env1=val1 env2=val2 ...");
// Add env settings in raw V1 syntax.
envobj.MergeFromV1Raw("env1=val1;env2=val2;...");
// Add env settings in input V1or2 syntax (in this example V2 input syntax).
envobj.MergeFromV1RaworV2Quoted("\"env1=val1 env2=val2 ...\"");
// Add a single environment value.
envobj.SetEnv("env1=val1");
// Add a single environment value that is already broken into key and value.
envobj.SetEnv("env1","val1");

The V2 environment syntax is preferable over V1, because it is opsys
independent and it supports quoting of special characters.  The V2
syntax is a sequence of whitespace delimited tokens processed by
split_args().  Each token is then split on the first '=' sign into
key=value pairs.

There is a "raw" format and a quoted format.  The quoted format is
designed to differentiate in a backward compatible way between input
strings that are in the old V1 syntax and the new V2 syntax.  The
quoted format simply means that V2 strings should be enclosed in
double-quotes and any literal double-quotes should be backwacked.

Example V2Raw syntax:
           env1='val one' 'env2=''val2''' env3="val3"
  yields {"env1" = "val one"}, {"env2" = "'val2'"}, {"env3" = "\"val3\""}

Example V2Quoted syntax yielding same as above:
           "env1='val one' 'env2=''val2''' env3=""val3"""

***********************************************************************/


#include "HashTable.h"
#include "MyString.h"
#include "condor_arglist.h"
#include "condor_classad.h"
#include "condor_ver_info.h"

class Env {
 public:
	Env();
	~Env();

		// Returns the number of environment entries.
	int Count() const;

		// Remove all environment entries.
	void Clear();

		// Add (or overwrite) environment entries from an input
		// string.  If the string begins with a double-quote, it will
		// be treated as V2Quoted; otherwise it will be read as V1Raw.
	bool MergeFromV1RawOrV2Quoted( const char *delimitedString, MyString *error_msg );

		// Add (or overwrite) environment entries from an input string.
		// If the string is not in V2Quoted format, this function
		// returns false and generates an error message.
	bool MergeFromV2Quoted( const char *delimitedString, MyString *error_msg );

		// Add (or overwrite) environment entries from an input string.
		// This should only be called for strings in raw V2 format.
	bool MergeFromV2Raw( const char *delimitedString, MyString *error_msg );

		// Add (or overwrite) environment entries from an input string.
		// This should only be called for strings in raw V1 format.
	bool MergeFromV1Raw( const char *delimitedString, MyString *error_msg );

		// Add (or overwrite) environment entries from an input string.
		// This should only be called for strings in raw V1or2 format,
		// which is designed to allow version detection in a backward
		// compatible way.
	bool MergeFromV1or2Raw( const char *delimitedString, MyString *error_msg );

		// Add (or overwrite) environment entries from a NULL-terminated
		// array of key=value strings.
	bool MergeFrom( char const * const *stringArray );

		// Add (or overwrite) environment entries from a NULL-delimited
		// character string
	bool MergeFrom( char const * );

		// Add (or overwrite) environment entries from another
		// environment object.
	void MergeFrom( Env const &env );

		// Add (or overwrite) environment entries from a ClassAd.
	bool MergeFrom( const ClassAd *ad, MyString *error_msg );

		// Add (or overwrite) a key=value environment entry.
	bool SetEnvWithErrorMessage( const char *nameValueExpr, MyString *error_msg );

		// Add (or overwrite) a key=value environment entry.
	bool SetEnv( const char *nameValueExpr ) {
		return SetEnvWithErrorMessage(nameValueExpr, NULL);
	}

		// Add (or overwrite) specified environment variable.
	bool SetEnv( const char *var, const char *val );

		// Add (or overwrite) specified environment variable.
	bool SetEnv( const MyString &, const MyString & );

		// Update ClassAd with new environment, possibly adjusting the
		// format depending on the Condor version and opsys of the
		// receiver.
	bool InsertEnvIntoClassAd( ClassAd *ad, MyString *error_msg, char const *opsys=NULL, CondorVersionInfo *condor_version=NULL ) const;

		// Returns true if specified condor version requires V1 env syntax.
	static bool CondorVersionRequiresV1(CondorVersionInfo const &condor_version);

		// Modern style: space delimited (and quoted as necessary).
		// If mark_v2=true, then result will be identifiable as V2 by
		// MergeV1or2()
	bool getDelimitedStringV2Raw(MyString *result,MyString *error_msg,bool mark_v2=false) const;

	 // old-style ; or | delimited
	bool getDelimitedStringV1Raw(MyString *result,MyString *error_msg,char delim='\0') const;

		// Return V1 string if possible, o.w. marked V2 string.
		// Sets this object's environment to that of ad, and uses
		// V1 delim from ad when constructing V1 result (so
		// opsys flavor of V1 environment is preserved).
	bool getDelimitedStringV1or2Raw(ClassAd const *ad,MyString *result,MyString *error_msg);

		// Returns V1 string if possible, o.w. marked V2 string.
	bool getDelimitedStringV1or2Raw(MyString *result,MyString *error_msg,char delim='\0') const;

		// Returns V2Quoted string (i.e. enclosed in double quotes).
	bool getDelimitedStringV2Quoted(MyString *result,MyString *error_msg) const;

		// Returns V1Raw if possible; o.w. V2Quoted.
		// In other words, retain backward-compatibility with older versions
		// of condor_submit if possible.
	bool getDelimitedStringV1RawOrV2Quoted(MyString *result,MyString *error_msg) const;

		// Get a string describing the environment in this Env object.
	void getDelimitedStringForDisplay(MyString *result) const;

		// Caller should delete the string.
		// Caller should delete string.
	char *getNullDelimitedString() const;

		// Returns a null-terminated array of strings.
		// Caller should delete it (e.g. with deleteStringArray()).
	char **getStringArray() const;

	bool GetEnv(MyString const &var,MyString &val) const;

		// Returns true if string is safe to insert in old-style
		// ; or | delimited string.
	static bool IsSafeEnvV1Value(char const *str,char delim='\0');

		// Returns true if string is safe to insert in new-style
		// environment string.
	static bool IsSafeEnvV2Value(char const *str);

		// Return the appropriate environment delimiter for this opsys.
	static char GetEnvV1Delimiter(char const *opsys=NULL);

		// Returns true if string is V2Quoted format.  In other words,
		// this checks that the string begins with a double-quote.
	static bool IsV2QuotedString(char const *str);

		// Convert a V2Quoted string to a V2Raw string.
		// (IsV2QuotedString() must be true or this will EXCEPT.)
	static bool V2QuotedToV2Raw(char const *v1_quoted,MyString *v2_raw,MyString *errmsg);

	bool InputWasV1() const {return input_was_v1;}

 protected:
	HashTable<MyString, MyString> *_envTable;
	bool input_was_v1;

	static bool ReadFromDelimitedString( char const *&input, char *output );

	static void WriteToDelimitedString(char const *input,MyString &output);

	static void AddErrorMessage(char const *msg,MyString *error_buffer);
};

#endif	// _ENV_H
