#include "utils.h"

#ifdef __linux__ 
string to_string(const wstring& wstr)
{
    return utf_to_utf<char>(wstr.c_str(), wstr.c_str() + wstr.size());
}
#elif _WIN32
string to_string(const wstring& wstr)
{
    typedef std::codecvt_utf8<wchar_t> convert_typeX;
    std::wstring_convert<convert_typeX, wchar_t> converterX;

    return converterX.to_bytes(wstr);
}
#endif

bool IsIpV4(const string& ip)
{
    ErrorCode err;
    IpAddress addr(IpAddress::from_string(ip, err));
    return !err ? true : false;
}