/*
 * (c) 2008, Bernhard Walle <bwalle@suse.de>, SUSE LINUX Products GmbH
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
#include <string>
#include <sstream>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <ctime>

#include "stringutil.h"
#include "global.h"
#include "debug.h"

#if HAVE_LIBSSL
#   include <openssl/bio.h>
#   include <openssl/evp.h>
#endif

using std::string;
using std::stringstream;
using std::strcpy;
using std::hex;
using std::setfill;
using std::setw;

//{{{ Stringutil ---------------------------------------------------------------

// -----------------------------------------------------------------------------
int Stringutil::string2number(const std::string &string)
    throw ()
{
    int ret;
    stringstream ss;
    ss << string;
    ss >> ret;
    return ret;
}

// -----------------------------------------------------------------------------
long long Stringutil::string2llong(const std::string &string)
    throw ()
{
    long long ret;
    stringstream ss;
    ss << string;
    ss >> ret;
    return ret;
}

// -----------------------------------------------------------------------------
char ** Stringutil::stringv2charv(const StringVector &strv)
    throw (KError)
{
    char **ret;

    // allocate the outer array
    ret = new char *[strv.size() + 1];
    if (!ret)
        throw KSystemError("Memory allocation failed", errno);
    ret[strv.size()] = NULL;

    // fill the inner stuff
    int i = 0;
    for (StringVector::const_iterator it = strv.begin();
            it != strv.end();
            ++it) {
        // don't use strdup() to avoid mixing of new and malloc
        ret[i] = new char[(*it).size() + 1];
        strcpy(ret[i], (*it).c_str());
        i++;
    }

    return ret;
}

/* -------------------------------------------------------------------------- */
ByteVector Stringutil::str2bytes(const string &string)
    throw ()
{
    ByteVector ret;
    const char *cstr = string.c_str();

    ret.insert(ret.begin(), cstr, cstr + string.size());

    return ret;
}

/* -------------------------------------------------------------------------- */
string Stringutil::bytes2str(const ByteVector &bytes)
    throw ()
{
    return string(bytes.begin(), bytes.end());
}

// -----------------------------------------------------------------------------
string Stringutil::bytes2hexstr(const char *bytes, size_t len, bool colons)
    throw ()
{
    stringstream ss;

    for (size_t i = 0; i < len; i++) {
        ss << setw(2) << setfill('0') << hex << int(int(bytes[i]) & 0xff);
        if (colons && i != (len-1))
            ss << ":";
    }

    return ss.str();
}

// -----------------------------------------------------------------------------
string Stringutil::vector2string(const StringVector &vector,
                                 const char *delimiter)
    throw ()
{
    string ret;

    for (size_t i = 0; i < vector.size(); i++) {
        ret += vector[i];
        if (i != vector.size() - 1)
            ret += delimiter;
    }

    return ret;
}

// -----------------------------------------------------------------------------
StringVector Stringutil::splitlines(const string &str)
    throw ()
{
    StringVector ret;
    stringstream ss;
    ss << str;

    string s;
    while (getline(ss, s))
        ret.push_back(s);

    return ret;
}

// -----------------------------------------------------------------------------
StringVector Stringutil::split(const string &string, char split)
    throw ()
{
    string::size_type currentstart = 0;
    string::size_type next;
    StringVector ret;

    next = string.find(split);
    while (next != string::npos) {
        ret.push_back(string.substr(currentstart, next-currentstart));
        currentstart = next+1;
        next = string.find(split, currentstart);
    }

    // rest
    ret.push_back(string.substr(currentstart));

    return ret;
}

// -----------------------------------------------------------------------------
string Stringutil::join(const StringVector &stringvector, char joinchar)
    throw ()
{
    string result;

    if (stringvector.size() == 0) {
        return "";
    }

    for (size_t i = 0; i < stringvector.size(); ++i) {
        if (i != 0) {
            result += joinchar;
        }
        result += stringvector[i];
    }

    return result;
}

// -----------------------------------------------------------------------------
string Stringutil::formatUnixTime(const char *formatstring, time_t value)
    throw ()
{
    char buffer[BUFSIZ];

    struct tm mytime;
    strftime(buffer, BUFSIZ, formatstring, localtime_r(&value, &mytime));

    return string(buffer);
}

// -----------------------------------------------------------------------------
string Stringutil::formatCurrentTime(const char *formatstring)
    throw ()
{
    return formatUnixTime(formatstring, time(NULL));
}

#if HAVE_LIBSSL

// -----------------------------------------------------------------------------
void Stringutil::digest_base64(const void *buf, size_t len,
			       char *md5sum, char *sha1sum)
    throw (KError)
{
	BIO *bio, *b64bio, *sha1bio, *md5bio;
	char tmpbuf[256];
	int res;

	bio = BIO_new_mem_buf(const_cast<void *> (buf), len);
	if (!bio)
	    throw KError("Cannot initialize memory stream");

	try {
	    b64bio = BIO_new(BIO_f_base64());
	    if (!b64bio)
		throw KError("Cannot initilaize base64 decoder");
	    BIO_set_flags(b64bio, BIO_FLAGS_BASE64_NO_NL);
	    bio = BIO_push(b64bio, bio);

	    sha1bio = BIO_new(BIO_f_md());
	    if (!sha1bio)
		throw KError("Cannot initialize MD5 sum");
	    BIO_set_md(sha1bio, EVP_sha1());
	    bio = BIO_push(sha1bio, bio);

	    md5bio = BIO_new(BIO_f_md());
	    if (!md5bio)
		throw KError("Cannot initialize SHA1 sum");
	    BIO_set_md(md5bio, EVP_md5());
	    bio = BIO_push(md5bio, bio);

	    while ( (res = BIO_read(md5bio, tmpbuf, sizeof tmpbuf)) > 0)
		;
	    if (res < 0)
		throw KError("Decoding failed");

	    if (BIO_gets(md5bio, md5sum, MD5SUM_LENGTH) != MD5SUM_LENGTH)
		throw KError("Error getting MD5 sum");

	    if (BIO_gets(sha1bio, sha1sum, SHA1SUM_LENGTH) != SHA1SUM_LENGTH)
		throw KError("Error getting SHA1 sum");

	} catch (...) {
	    BIO_free_all(bio);
	    throw;
	}

	BIO_free_all(bio);
}

#endif	// HAVE_LIBSSL

// -----------------------------------------------------------------------------
int Stringutil::hex2int(char c)
    throw (KError)
{
    if (c >= '0' && c <= '9')
	return c - '0';
    if (c >= 'a' && c <= 'f')
	return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
	return c - 'A' + 10;
    throw KError(string("Stringutil::hex2int: '") + c + "' is not a hex digit");
}

//}}}
//{{{ KString ------------------------------------------------------------------

// -----------------------------------------------------------------------------
bool KString::isNumber()
    throw ()
{
    for (size_t i = 0; i < size(); ++i) {
        char c = operator[](i);

        // leading sign
        if (i == 0 && (c == '-' || c == '+')) {
            continue;
        } else {
            if (!isdigit(c)) {
                return false;
            }
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
KString& KString::trim(const char *chars)
    throw ()
{
    return rtrim(chars).ltrim(chars);
}

// -----------------------------------------------------------------------------
KString& KString::ltrim(const char *chars)
    throw ()
{
    erase(0, find_first_not_of(chars));
    return *this;
}

// -----------------------------------------------------------------------------
KString& KString::rtrim(const char *chars)
    throw ()
{
    erase(find_last_not_of(chars) + 1);
    return *this;
}

// -----------------------------------------------------------------------------
bool KString::startsWith(const string &part) const
    throw ()
{
    return strncmp(c_str(), part.c_str(), part.size()) == 0;
}

// -----------------------------------------------------------------------------
bool KString::endsWith(const string &part) const
    throw ()
{
    if (part.size() > size())
        return false;

    return strcmp(c_str() + size() - part.size(), part.c_str()) == 0;
}

// -----------------------------------------------------------------------------
KString &KString::decodeURL(bool formenc)
    throw()
{
    iterator src, dst;
    for (src = dst = begin(); src != end(); ++src) {
	char c1, c2;
	if (*src == '%' && end() - src >= 2 &&
	    isxdigit(c1 = src[1]) && isxdigit(c2 = src[2])) {
	    *dst++ = (Stringutil::hex2int(c1) << 4) |
		Stringutil::hex2int(c2);
	    src += 2;
	} else if (formenc && *src == '+')
	    *dst++ = ' ';
	else
	    *dst++ = *src;
    }
    resize(dst - begin());
    return *this;
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
