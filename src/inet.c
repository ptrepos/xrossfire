#include <winsock2.h>
#include <stdbool.h>
#include <limits.h>
#include <xrossfire/base.h>
#include <xrossfire/string.h>
#include <xrossfire/net.h>

static bool xf_xchar_to_digit10(xf_char_t c, /*out*/int *i);
static bool xf_xchar_to_digit16(xf_char_t c, /*out*/int *i);

static bool xf_xchar_to_digit10(xf_char_t c, /*out*/int *i)
{
    if (_T('0') <= c && c <= _T('9')) {
        *i = c - _T('0');
        return true;
    }

    return false;
}

static int xf_string_parse_digit10(xf_string_t *s, /*inout*/int *offset)
{
    int value = 0;
    int digits;
    while (xf_xchar_to_digit10(s->buf[*offset], /*out*/&digits) == false) {
        value = value * 10 + digits;
        *offset += 1;
    }

    return value;
}

XROSSFIRE_PRIVATE bool xf_inet_parse_ipv4(xf_string_t *s, /*out*/struct in_addr *in_addr)
{
    int value = 0;
    int section = 0;

    int offset = 0;

    int b1 = xf_string_parse_digit10(s, /*out*/&offset);
    if (b1 < 0 || 255 < b1)
        goto FAIL;
    if (s->buf[offset] != '.') {
        goto FAIL;
    }
    
    offset += 1;

    int b2 = xf_string_parse_digit10(s, /*out*/&offset);
    if (b2 < 0 || 255 < b2)
        goto FAIL;
    if (s->buf[offset] != '.') {
        goto FAIL;
    }

    int b3 = xf_string_parse_digit10(s, /*out*/&offset);
    if (b3 < 0 || 255 < b3)
        goto FAIL;
    if (s->buf[offset] != '.') {
        goto FAIL;
    }

    int b4 = xf_string_parse_digit10(s, /*out*/&offset);
    if (b4 < 0 || 255 < b4)
        goto FAIL;

    if (xf_string_get_length(s) != offset) {
        goto FAIL;
    }

    in_addr->S_un.S_un_b.s_b1 = (unsigned char)b1;
    in_addr->S_un.S_un_b.s_b2 = (unsigned char)b2;
    in_addr->S_un.S_un_b.s_b3 = (unsigned char)b3;
    in_addr->S_un.S_un_b.s_b4 = (unsigned char)b4;

    return true;
FAIL:
    return false;
}

static bool xf_xchar_to_digit16(xf_char_t c, /*out*/int *i)
{
    if (_T('0') <= c && c <= _T('9')) {
        *i = c - _T('0');
        return true;
    } else if (_T('A') <= c && c <= _T('F')) {
        *i = c - _T('A') + 10;
        return true;
    } else if (_T('a') <= c && c <= _T('f')) {
        *i = c - _T('a') + 10;
        return true;
    }

    return false;
}

static int xf_string_parse_digit16(xf_string_t *s, /*inout*/int *offset)
{
    int value = 0;
    int digits;
    while (xf_xchar_to_digit16(s->buf[*offset], /*out*/&digits) == false) {
        value = value * 10 + digits;
        *offset += 1;
    }

    return value;
}

XROSSFIRE_PRIVATE bool xf_inet_parse_ipv6(xf_string_t *s, /*out*/struct in6_addr *in_addr)
{
    int offset = 0;

    for (int i = 0; i < 7; i++) {
        int w0 = xf_string_parse_digit16(s, /*out*/&offset);
        if (w0 < 0 || USHRT_MAX < w0)
            goto FAIL;
        if (s->buf[offset] != ':') {
            goto FAIL;
        }

        in_addr->u.Word[i] = (unsigned short)w0;
    }

    int w7 = xf_string_parse_digit16(s, /*out*/&offset);
    if (w7 < 0 || USHRT_MAX < w7)
        goto FAIL;

    in_addr->u.Word[7] = (unsigned short)w7;

    return true;
FAIL:
    return false;
}

XROSSFIRE_PRIVATE bool xf_inet_parse(xf_string_t *s, int *ip_type, /*out*/struct in_addr *in_addr, /*out*/struct in6_addr *in6_addr)
{
    if (xf_inet_parse_ipv4(s, in_addr)) {
        *ip_type = XF_IPV4;
        return true;
    } else if (xf_inet_parse_ipv6(s, in6_addr)) {
        *ip_type = XF_IPV6;
        return true;
    } else {
        return false;
    }
}
